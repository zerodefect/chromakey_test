# chromakey_test
A test of FFmpeg's chromakey libavfilter

Once built (only tested on Linux with g++ v8.4), the application accepts two parameters:
* The input image file path
* The output raw planar data. (YUV422p)

The output raw planar data can be loaded at rawpixels.net
