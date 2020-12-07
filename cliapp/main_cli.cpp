// Contents released under the The MIT License (MIT)

#include "psdisc-filesystem.h"
#include "psdisc-cdvd-image.h"

#include "icy_assert.h"
#include "icy_log.h"
#include "jfmt.h"
#include "posix_file.h"

#include <cstdio>


static bool bVerbose = 1;

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

    log_host( "AddFile [parent=%-6jd sector=%-6jd len=%-10jd]: %s", JFMT(parent), JFMT(secstart),
        //(len < 1024*1024)
        //  ? cFmtStr("%jd",    JFMT(len))
        //  : cFmtStr("%.3fMB", (double)len / (1024*1024)),
        JFMT(len),
        name
    );

    if (m_filesBySector.count(secstart)) {
        if (bVerbose) {
            log_host("    (Duplicate)");
        }
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

void DetectIsoBinFileType() {
}

struct MediaReaderIfc {

    MediaSourceDescriptor media;
    PsDiscFn_ioReadP io_read_cb;

    bool ReadData2048        (uint8_t* dest, psdisc_off_t sector, psdisc_off_t offset, psdisc_off_t length);
    bool ReadSector2336      (uint8_t* dest, psdisc_off_t sector, psdisc_off_t length);
    bool ReadSectorDigitalAudio2352(uint8_t* dest, psdisc_off_t sector, psdisc_off_t length);
};

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
        auto res = io_read_cb(dest, media.sector_size * sec_read_count, read_offset);
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
            auto res = io_read_cb(wptr, 2048, read_offset);
            if(!res) {
                log_host("ERROR: ReadData2048(seekpos=%jd)", JFMT(read_offset));
                return false;
            }
        }
    }

    return true;
}

int main(int argc, char* argv[]) {

    PsDiscDirParser parser;

    int fd = posix_open(argv[1], O_RDONLY, _S_IREAD | _S_IWRITE);

    MediaReaderIfc ifc = {};
    ifc.io_read_cb = [&](void* dest, intmax_t count, intmax_t pos) {
        return posix_pread(fd, dest, count, pos);
    };

    auto statres = posix_fstat(fd);
    ifc.media.image_size = statres.st_size;
    DiscFS_DetectMediaDescription(ifc.media, fd);

    //parser.read_data_cb = &ifc.ReadData2048;

    parser.read_data_cb = [&](uint8_t* dest, psdisc_off_t sector, psdisc_off_t offset, psdisc_off_t length) {
        return ifc.ReadData2048(dest, sector, offset, length);
    };

    parser.ReadFilesystem(AddFile);
}
