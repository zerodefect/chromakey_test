#include <cassert>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>

#include <boost/scope_exit.hpp>
#include <sstream>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/pixdesc.h>
}

//////////////////////////////////////////////////////////////////////////
typedef std::unique_ptr<AVFrame, std::function<void(AVFrame*)> > AVFrame_unique_ptr;
typedef std::unique_ptr<AVFilterGraph, std::function<void(AVFilterGraph*)> > AVFilterGraph_unique_ptr;

//////////////////////////////////////////////////////////////////////////
std::string error_code_to_string(const int nErrCode)
{
    char chArray[AV_ERROR_MAX_STRING_SIZE];

    // GAV: Probably better to be on the safe size and initialise the array.
    // The docs do not state if the string is null terminated :(
    std::fill(std::begin(chArray), std::end(chArray), '\0');

    if (av_strerror(nErrCode, chArray, AV_ERROR_MAX_STRING_SIZE) != 0)
    {
        return "[Unknown]";
    }

    return std::string(chArray);
}

///////////////////////////////////////////////////////////////////////////
bool open_input_format_context(const std::string &pathAsset,
                               std::unique_ptr<AVFormatContext, std::function<void (AVFormatContext*)>> &out_apFmtCtx)
{
    out_apFmtCtx.reset();

    AVFormatContext *pFmtCtx = nullptr;

    // Open input file, and allocate format context
    if (int nRet = avformat_open_input(&pFmtCtx,
                                             pathAsset.c_str(),
                                             nullptr,
                                             nullptr); nRet < 0)
    {
        std::cerr << "Could not open media at path: '"
                  << pathAsset
                  << "': "
                  << nRet
                  << std::endl;

        return false;
    }

    out_apFmtCtx = std::unique_ptr<AVFormatContext, std::function<void(AVFormatContext *)>>{pFmtCtx,
                                                                                            [](AVFormatContext *pFmtCtx)
                                                                                            {
                                                                                                if (pFmtCtx != nullptr)
                                                                                                {
                                                                                                    avformat_close_input(&pFmtCtx);
                                                                                                }
                                                                                            }};
    return true;
}

//////////////////////////////////////////////////////////////////////////
bool load_image(AVFrame_unique_ptr &apAVFrame,
                const std::string &pathAsset,
                std::string &out_strError)
{
    out_strError.clear();

    std::unique_ptr<AVFormatContext, std::function<void (AVFormatContext*)>> apFmtCtx;
    if (bool bOk = open_input_format_context(pathAsset, apFmtCtx); !bOk) { return false; }

    int idx = 0;
    if ((idx = av_find_best_stream(apFmtCtx.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0)) < 0)
    {
        out_strError = "Failed to find video stream for '" + pathAsset + "'";
        return false;
    }

    AVCodecParameters *par = apFmtCtx->streams[idx]->codecpar;
    if (!par)
    {
        out_strError = "Failed to retrieve codec parameters: NULL codec parameters pointer";
        return false;
    }

    AVCodec *pCodec = avcodec_find_decoder(par->codec_id);
    if (!pCodec)
    {
        out_strError = "Failed to find codec: NULL codec pointer";
        return false;
    }

    const std::unique_ptr<AVCodecContext, std::function<void(AVCodecContext*)>> apCodecCtx =
        {
            avcodec_alloc_context3(pCodec),
            [](AVCodecContext *pCodecCtx)
            {
                if (pCodecCtx != nullptr) { avcodec_free_context(&pCodecCtx); }
            }
        };

    if (int ret = avcodec_parameters_to_context(apCodecCtx.get(), par); ret < 0)
    {
        out_strError = "Failed to copy codec parameters to decoder context: code=" + std::to_string(ret);
        return false;
    }

    apCodecCtx->skip_alpha = 0;
    apCodecCtx->pix_fmt = AV_PIX_FMT_YUVA420P;
    apCodecCtx->sw_pix_fmt = AV_PIX_FMT_YUVA420P;

    if (int ret = avcodec_open2(apCodecCtx.get(), pCodec, nullptr); ret < 0)
    {
        out_strError = "Failed to open codec: code=" + std::to_string(ret);
        return false;
    }

    if (!apAVFrame)
    {
        apAVFrame = AVFrame_unique_ptr{av_frame_alloc(),
                                          [](AVFrame *pAVFrame) { av_frame_free(&pAVFrame); }};
    }

    AVPacket pkt={};
    av_init_packet(&pkt);

    if (int ret = av_read_frame(apFmtCtx.get(), &pkt); ret < 0)
    {
        out_strError = "Failed to read frame from file: code=" + std::to_string(ret);
        return false;
    }

    ///////////////////////////////////////////////
    // RAII AVPacket clean-up.
    //////////////////////////////////////////////
    BOOST_SCOPE_EXIT(&pkt)
    {
        av_packet_unref(&pkt);

        // This should now be null.
        assert(pkt.data == nullptr);
    } BOOST_SCOPE_EXIT_END

    if (int ret = avcodec_send_packet(apCodecCtx.get(), &pkt); ret < 0)
    {
        out_strError = "Failed to decode image from file: " + std::to_string(ret);
        return false;
    }

    if (int ret = avcodec_receive_frame(apCodecCtx.get(), apAVFrame.get()); ret < 0)
    {
        out_strError = "Failed to decode image from file: code=" + std::to_string(ret);
        return false;
    }

    /*std::cout << "Format: "
              << av_get_pix_fmt_name(static_cast<AVPixelFormat>(apAVFrame->format))
              << ", Value: " 
              << apAVFrame->format
              << std::endl;*/

    return true;
}

///////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
    std::cout << "Chromakey Test v0.1. Published by Gavin Smith." << std::endl;

    /// WARN:
    // Depending on the version of FFmpeg the following function call may or may not be required
    av_register_all();
    avfilter_register_all();

    if (argc < 2)
    {
        std::cerr << "No input image path specified." << std::endl;
        return 1;
    }

    if (argc < 3)
    {
        std::cerr << "No output RAW file path specified." << std::endl;
        return 1;
    }

    std::string strError;
    AVFrame_unique_ptr apAVFrame;
    if (bool bOk = load_image(apAVFrame, argv[1], strError); !bOk)
    {
        std::cerr << "Failed to load image. " << strError << std::endl;
        return 1;
    }

    static std::ofstream ofile(argv[2], std::ios::binary | std::ios::out);
    if (!ofile.is_open())
    {
        std::cerr << "Failed to open output RAW file. " << strError << std::endl;
        return 1;
    }

    std::cout << "Input Image File: " << argv[1] << std::endl;
    std::cout << "Output RAW File: " <<argv[2] <<  std::endl;

    AVFilterGraph_unique_ptr apFilterGraph = AVFilterGraph_unique_ptr(avfilter_graph_alloc(),
                                                                      [](AVFilterGraph *pGraph)
                                                                      {
                                                                          if (pGraph != nullptr)
                                                                          {
                                                                              avfilter_graph_free(&pGraph);
                                                                          }
                                                                      });

    AVFilterContext *pBufferSrcCtx{nullptr};

    // Buffer video src: the start of the filter chain where frames will be pushed.
    {
        std::ostringstream osstr;

        osstr << "width="
              << apAVFrame->width
              << ":height="
              << apAVFrame->height
              << ":pix_fmt="
              << av_get_pix_fmt_name((AVPixelFormat)apAVFrame->format)
              << ":time_base=1/25";

        const auto strArgs = osstr.str();

        auto *pFilter{avfilter_get_by_name("buffer")};
        if (int ret = avfilter_graph_create_filter(&pBufferSrcCtx,
                                                   pFilter,
                                                   "buffersrc",
                                                   strArgs.c_str(),
                                                   nullptr,
                                                   apFilterGraph.get()); ret < 0)
        {
            std::cerr << "Cannot create buffer filter (args: "
                      << osstr.str()
                      << " ) - code="
                      << ret
                      << ", error=\""
                      << error_code_to_string(ret)
                      << "\""
                      << std::endl;
            return 1;
        }
    }

    AVFilterContext *pBufferSinkCtx{nullptr};

    // Buffer video sink: the end of the filter chain where frames will be pulled.
    {
        if (int ret = avfilter_graph_create_filter(&pBufferSinkCtx,
                                                   avfilter_get_by_name("buffersink"),
                                                   "buffersink",
                                                   "",
                                                   nullptr,
                                                   apFilterGraph.get()); ret < 0)
        {
            std::cerr << "Cannot create buffersink filter - code="
                      << ret
                      << std::endl;
            return 1;
        }
    }

    AVFilterContext *pFormatCtx{nullptr};

    // Format
    {
        std::string strArgs = "pix_fmts=yuv422p";

        if (int ret = avfilter_graph_create_filter(&pFormatCtx,
                                                   avfilter_get_by_name("format"),
                                                   "format",
                                                   strArgs.c_str(),
                                                   nullptr,
                                                   apFilterGraph.get()); ret < 0)
        {
            std::cerr << "Cannot create format filter (args: "
                      << strArgs
                      << " ) - code="
                      << ret
                      << std::endl;
            return 1;
        }
    }

    AVFilterContext *pChromaKeyCtx{nullptr};

    // Chromakey
    {
        // Note: I've tried the following parameters to key green to transparency:
        auto *pchArgs = "color=green:similarity=0.3:blend=0.3";
        //auto *pchArgs = "color=0x00FF00:similarity=0.3:blend=0.3";
        //auto *pchArgs = "color=0x952B15:similarity=0.3:blend=0.3:yuv=1"; // RGB2YUV conversion of green - assumes Y, U, and then V.

        if (int ret = avfilter_graph_create_filter(&pChromaKeyCtx,
                                                   avfilter_get_by_name("chromakey"),
                                                   "chromakey",
                                                   pchArgs,
                                                   nullptr,
                                                   apFilterGraph.get()); ret < 0)
        {
            std::cerr << "Cannot create chromakey filter (args: "
                      << pchArgs
                      << " ) - code="
                      << ret
                      << std::endl;
            return 1;
        }
    }

    // Link buffer to format
    if (const auto ret = avfilter_link(pBufferSrcCtx, 0, pFormatCtx, 0); ret < 0)
    {
        std::cerr << "Error connecting format buffer source to format: code="
                  << ret
                  << std::endl;
        return 1;
    }

    // Link format to chroma key
    if (const auto ret = avfilter_link(pFormatCtx, 0, pChromaKeyCtx, 0); ret < 0)
    {
        std::cerr << "Error connecting format to chroma key: code="
                  << ret
                  << std::endl;
        return 1;
    }

    // Link Chroma key to buffer sink
    if (const auto ret = avfilter_link(pChromaKeyCtx, 0, pBufferSinkCtx, 0); ret < 0)
    {
        std::cerr << "Error connecting chroma key to buffer sink: code="
                  << ret
                  << std::endl;
        return 1;
    }

    if (const int ret = avfilter_graph_config(apFilterGraph.get(), nullptr); ret < 0)
    {
        std::cerr << "Failed to configure the graph: code="
                  << ret
                  << std::endl;
        return 1;
    }

    if (const auto ret = av_buffersrc_add_frame_flags(pBufferSrcCtx,
                                                      apAVFrame.get(),
                                                      AV_BUFFERSRC_FLAG_KEEP_REF | AV_BUFFERSRC_FLAG_PUSH); ret < 0)
    {
        std::cerr << "Failed to add a frame to the graph: code="
                  << ret
                  << std::endl;
        return 1;
    }

    AVFrame_unique_ptr apAVFrameOut
        (
            av_frame_alloc(),
            [](AVFrame *p) { av_frame_free(&p); }
        );

    if (auto ret = av_buffersink_get_frame(pBufferSinkCtx, apAVFrameOut.get()); ret < 0)
    {
        std::cerr << "Failed to retrieve a frame from the graph: code="
                  << ret
                  << std::endl;
        return 1;
    }

    for (std::size_t k = 0; k < 4; ++k)
    {
        ofile.write((const char *) apAVFrameOut->data[k], apAVFrameOut->linesize[k] * apAVFrameOut->height);
    }
    ofile.close();

    std::cout << "Completed successfully. Exiting." << std::endl;
    return 0;
}

