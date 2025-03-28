/**
 * ----------------------------------------------------------------------------
 * Company    : SLAC National Accelerator Laboratory
 * ----------------------------------------------------------------------------
 * Description :
 *    Class to coordinate data file writing.
 *    This class supports multiple stream slaves, each with the ability to
 *    write to a common data file. The data file is a series of banks.
 *    Each bank has a channel and frame flags. The channel is per source and the
 *    lower 24 bits of the frame flags are used as the flags.
 *    The bank is proceeded by 2 - 32-bit headers to indicate bank information
 *    and length:
 *
 *       headerA:
 *          [31:0] = Length of data block in bytes
 *       headerB
 *          31:24  = Channel ID
 *          23:16  = Frame error
 *          15:0   = Frame flags
 *
 *    Optionally a flag can be set which writes the raw frame data directly to the file.
 *
 *-----------------------------------------------------------------------------
 * This file is part of the rogue software platform. It is subject to
 * the license terms in the LICENSE.txt file found in the top-level directory
 * of this distribution and at:
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 * No part of the rogue software platform, including this file, may be
 * copied, modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 *-----------------------------------------------------------------------------
 **/
#ifndef __ROGUE_UTILITIES_FILEIO_STREAM_WRITER_H__
#define __ROGUE_UTILITIES_FILEIO_STREAM_WRITER_H__
#include "rogue/Directives.h"

#include <stdint.h>

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "rogue/EnableSharedFromThis.h"
#include "rogue/Logging.h"
#include "rogue/interfaces/stream/Frame.h"

namespace rogue {
namespace utilities {
namespace fileio {

class StreamWriterChannel;

//! Stream writer central class
class StreamWriter : public rogue::EnableSharedFromThis<rogue::utilities::fileio::StreamWriter> {
    friend class StreamWriterChannel;

  protected:
    // Log
    std::shared_ptr<rogue::Logging> log_;

    //! File descriptor
    int32_t fd_;

    //! Base file name
    std::string baseName_;

    //! Current file index
    bool isOpen_;

    //! Current file index
    uint32_t fdIdx_;

    //! Size limit for auto close and re-open when limit is exceeded, zero to disable
    uint64_t sizeLimit_;

    //! Current file size in bytes
    uint64_t currSize_;

    //! Total file size in bytes
    uint64_t totSize_;

    //! Buffering size to cache file writes, zero if disabled
    uint32_t buffSize_;

    //! Write buffer
    uint8_t* buffer_;

    //! Write buffer count
    uint32_t currBuffer_;

    //! Drop errors flag
    bool dropErrors_;

    //! File access lock
    std::mutex mtx_;

    //! Total number of frames in file
    uint32_t frameCount_;

    //! Internal method for file writing
    void intWrite(void* data, uint32_t size);

    //! Check file size for next write
    void checkSize(uint32_t size);

    //! Flush file
    void flush();

    //! Write raw data
    bool raw_;

    //! condition
    std::condition_variable cond_;

    std::map<uint32_t, std::shared_ptr<rogue::utilities::fileio::StreamWriterChannel>> channelMap_;

    //! Write data to file. Called from StreamWriterChannel
    virtual void writeFile(uint8_t channel, std::shared_ptr<rogue::interfaces::stream::Frame> frame);

  public:
    //! Class creation
    static std::shared_ptr<rogue::utilities::fileio::StreamWriter> create();

    //! Setup class in python
    static void setup_python();

    //! Creator
    StreamWriter();

    //! Deconstructor
    virtual ~StreamWriter();

    //! Open a data file
    void open(std::string file);

    //! Close a data file
    void close();

    //! Get open status
    bool isOpen();

    //! Set raw mode
    void setRaw(bool raw);

    //! Get raw mode flag
    bool getRaw();

    //! Set buffering size, 0 to disable
    void setBufferSize(uint32_t size);

    //! Set max file size, 0 for unlimited
    void setMaxSize(uint64_t size);

    //! Set drop errors flag
    void setDropErrors(bool drop);

    //! Get a port
    std::shared_ptr<rogue::utilities::fileio::StreamWriterChannel> getChannel(uint8_t channel);

    //! Get total file size
    uint64_t getTotalSize();

    //! Get current file size
    uint64_t getCurrentSize();

    //! Get current frame count
    uint32_t getFrameCount();

    //! Block until a frame count is reached
    bool waitFrameCount(uint32_t count, uint64_t timeout);
};

// Convenience
typedef std::shared_ptr<rogue::utilities::fileio::StreamWriter> StreamWriterPtr;
}  // namespace fileio
}  // namespace utilities
}  // namespace rogue
#endif
