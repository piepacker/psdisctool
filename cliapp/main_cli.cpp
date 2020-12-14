// Contents released under the The MIT License (MIT)

#include "psdisc-filesystem.h"
#include "psdisc-cdvd-image.h"

#include "icy_assert.h"
#include "icy_log.h"
#include "jfmt.h"
#include "posix_file.h"

#include "StringTokenizer.h"
#include "defer.h"

#include <cstdio>


// verbose information is logged to stderr to avoid corrupting stdout behavior.
// (stdout information may be used by other scripts in automation pipeline)
static bool g_bVerbose = 0;

struct fileEnt_t
{
    psdisc_off_t    start_sector;
    psdisc_off_t    parent_sector;
    psdisc_off_t    len_bytes;
    int             type;
    char            name[kPsDiscMaxFileNameLength+1];

    bool isRoot() const {
        return parent_sector == 0;
    }
};


using UDF_FilesBySector  = std::map <psdisc_off_t,fileEnt_t>;

UDF_FilesBySector m_filesBySector;


void AddFile(psdisc_off_t secstart, psdisc_off_t len, int type, const uint8_t* name, int nameLen, psdisc_off_t parent)
{
    dbg_check( nameLen <= kPsDiscMaxFileNameLength );

    bool isDup = m_filesBySector.count(secstart);

    if (g_bVerbose) {
        log_error( "AddFile [parent=%-6jd sector=%-6jd len=%-10jd]: %s",
            JFMT(parent), JFMT(secstart), JFMT(len), name
        );
    }

    if (m_filesBySector.count(secstart)) {
        log_error("Suspicious duplicate encountered [parent=%-6jd sector=%-6jd len=%-10jd]: %s",
            JFMT(parent), JFMT(secstart), JFMT(len), name
        );

        return;
    }

    fileEnt_t fe;

    fe.start_sector     = secstart;
    fe.parent_sector    = parent;
    fe.len_bytes        = len;
    fe.type             = type;
    memcpy(fe.name, name, nameLen);
    fe.name[nameLen] = 0;

    // strip the ECMA-119 semicolon revision info.
    if (fe.name[nameLen-2] == ';') {
        fe.name[nameLen-2]  = 0;
    }

    m_filesBySector.insert({secstart, fe});
    //m_filesBySector.size();
}

// Plan here is to continue to flesh this out and (eventually) move it into the library once the API
// feels stable.
struct MediaReaderIfc {
    MediaSourceDescriptor media;
    PsDisc_IO_Interface   ioifc;

    void Construct                  (psdisc_off_t imgsize_in_bytes, PsDisc_IO_Interface ioInterface);

    bool ReadData2048               (uint8_t* dest, psdisc_off_t sector, psdisc_off_t offset, psdisc_off_t length);
    bool ReadSector2336             (uint8_t* dest, psdisc_off_t sector, psdisc_off_t length);
    bool ReadSectorDigitalAudio2352 (uint8_t* dest, psdisc_off_t sector, psdisc_off_t length);
};

void MediaReaderIfc::Construct(psdisc_off_t imgsize_in_bytes, PsDisc_IO_Interface ioInterface) {
    dbg_check(ioInterface.pread_cb, "pread callback is required for all media operations.");

    *this = {};
    ioifc = ioInterface;
    media.image_size = imgsize_in_bytes;
}

// Always reads 2048 bytes per sector. Assumes data mode operation only. Disregards the CDROM
// audio tracks sector mode(s). 2048 bytes is the ISO standard for data tracks, and in the case
// of .iso format is the only data present since the .iso format supports only data tracks.
bool MediaReaderIfc::ReadData2048(uint8_t* dest, psdisc_off_t sector, psdisc_off_t offset, psdisc_off_t length) {
    dbg_check(dest);

    if (!length)  return 1;
    if (!dest)    return 0;

    auto sec_read_count = (length + 2047) / 2048;

    if ((sector + sec_read_count) > media.num_sectors) {
        log_host("ERROR: ReadData2048(sector=%jd, len=%jd): read past end of media", JFMT(sector), JFMT(length));
        return 0;
    }


    auto read_offset = sector * media.sector_size;

    read_offset += media.offset_file_header;
    read_offset += media.offset_sector_leadin;      // data can skip the sync pattern, address, and mode info

    if (media.sector_size == 2048) {
        // optimized fastpath for ISO media.
        auto res = ioifc.pread_cb(dest, media.sector_size * sec_read_count, read_offset);
            if(!res) {
                log_host("ERROR: ReadData2048(seekpos=%jd)", JFMT(read_offset));
                return false;
            }
    }
    else {
        auto    todo = sec_read_count;
        auto*   wptr = dest;
        auto    end_sector = sector + sec_read_count;

        for(;sector < end_sector; ++sector, read_offset += media.sector_size) {
            auto res = ioifc.pread_cb(wptr, 2048, read_offset);
            if(!res) {
                log_host("ERROR: ReadData2048(seekpos=%jd)", JFMT(read_offset));
                return false;
            }
        }
    }

    return true;
}

#include "fs.h"

std::map<std::string, const char*> g_cli_options;
std::vector<fs::path> g_positional_files;

void cliApplyOption(const char* lvalue, const char* rvalue) {
    g_cli_options[lvalue] = rvalue;
}

bool cliHasOption(const char* lvalue) {
    return g_cli_options.find(lvalue) != g_cli_options.end();
}

bool cliGetOptionBool(const char* lvalue, bool defIsTrue=1) {
    if (auto rit = g_cli_options.find(lvalue); rit != g_cli_options.end()) {
        if (auto rvalue = rit->second) {
            if (!isdigit(rvalue[0])) {
                log_error("invalid rvalue assignment for boolean: %s=%s", lvalue, rvalue);
                return 0;
            }
            return rvalue[0] != '0';
        }
        return defIsTrue;
    }
    return 0;
}

int main(int argc, char* argv[]) {

    auto exename = fs::path(argv[0]).filename();
    
    // doesn't really make sense to support stdin for a disc image tool, due to the necessity of seeking
    // all over the image in order to get the info we need for even a simple directly listing.
    //bool has_stdin = ((fseek(stdin, 0, SEEK_END), ftell(stdin)) > 0);
    //rewind(stdin);

    bool end_of_switches = 0;       // set to 1 by '--' positional arg
    for(int i=1; i<argc; ++i) {
        auto* curarg = argv[i];
        if (!curarg || !curarg[0]) continue;

        if (!end_of_switches) {
            if (curarg[0] == '-' && curarg[1] == '-') {      //--setting
                if (!curarg[2]) {
                    end_of_switches = 1;
                }
                else {
                    auto tok = Tokenizer(curarg + 2);
                    auto* lvalue = tok.GetNextToken('=');
                    auto* rvalue = tok.GetNextToken();

                    cliApplyOption(lvalue, rvalue);
                    continue;
                }
            }
        }

        // must be a positional file input.
        g_positional_files.push_back(curarg);
    }

    g_bVerbose = cliGetOptionBool("verbose");

    if (cliHasOption("help")) {
        // RECONSIDER - it might be a better idea to have several different tools, eg:
        //     - psdisc-ls       - list dirs, other info
        //     - psdisc-extract  - extract file(s)
        printf("psdisctool - extract information, filesystem, and file contents from PS1/PS2 disc image.\n");
        exit(0);
    }

    if (g_positional_files.empty()) {
        log_error("No input files specified.");
        log_error("Specify '%s --help' for usage information.", exename.c_str());
        exit(1);
    }

    for(const auto& filename : g_positional_files) {
        int fd = posix_open(filename, O_RDONLY, _S_IREAD | _S_IWRITE);
        if (fd < 0) {
            log_error("%s: %s", strerror(errno), filename.c_str());
            continue;
        }
        Defer(posix_close(fd));

        PsDisc_IO_Interface ioifc = {
            // pread
            [&](void* dest, intmax_t count, intmax_t pos) {
                return posix_pread(fd, dest, count, pos);
            }
        };

        auto statres = posix_fstat(fd);

        MediaReaderIfc ifc;
        ifc.Construct(statres.st_size, ioifc);
        if (!DiscFS_DetectMediaDescription(ifc.media, fd)) {
            log_error("Could not parse contents of file: %s", filename.c_str());
            continue;
        }

        printf("(ls) %s:\n", filename.c_str());
        PsDiscDirParser parser;
        parser.read_data_cb = [&](uint8_t* dest, psdisc_off_t sector, psdisc_off_t offset, psdisc_off_t length) {
            return ifc.ReadData2048(dest, sector, offset, length);
        };      
        parser.ReadFilesystem(AddFile);
    }
}
