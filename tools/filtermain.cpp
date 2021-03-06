/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkDevice.h"
#include "SkGraphics.h"
#include "SkImageEncoder.h"
#include "SkPicture.h"
#include "SkPicturePlayback.h"
#include "SkPictureRecord.h"
#include "SkStream.h"

static void usage() {
    SkDebugf("Usage: filter -i inFile [-o outFile] [-t textureDir] [-h|--help]");
    SkDebugf("\n\n");
    SkDebugf("    -i inFile  : file to file.\n");
    SkDebugf("    -o outFile : result of filtering.\n");
    SkDebugf("    -t textureDir : directory in which to place textures.\n");
    SkDebugf("    -h|--help  : Show this help message.\n");
}

// SkFilterRecord allows the filter to manipulate the read in SkPicture
class SkFilterRecord : public SkPictureRecord {
public:
    SkFilterRecord(uint32_t recordFlags, SkDevice* device)
        : INHERITED(recordFlags, device)
        , fTransSkipped(0)
        , fTransTot(0)
        , fScalesSkipped(0)
        , fScalesTot(0) {
    }

    virtual bool translate(SkScalar dx, SkScalar dy) SK_OVERRIDE {
        ++fTransTot;

        if (0 == dx && 0 == dy) {
            ++fTransSkipped;
            return true;
        }

        return INHERITED::translate(dx, dy);
    }

    virtual bool scale(SkScalar sx, SkScalar sy) SK_OVERRIDE {
        ++fScalesTot;

        if (SK_Scalar1 == sx && SK_Scalar1 == sy) {
            ++fScalesSkipped;
            return true;
        }

        return INHERITED::scale(sx, sy);
    }

    void saveImages(const SkString& path) {
        SkTRefArray<SkBitmap>* bitmaps = fBitmapHeap->extractBitmaps();

        if (NULL != bitmaps) {
            for (int i = 0; i < bitmaps->count(); ++i) {
                SkString filename(path);
                if (!path.endsWith("\\")) {
                    filename.append("\\");
                }
                filename.append("image");
                filename.appendS32(i);
                filename.append(".png");

                SkImageEncoder::EncodeFile(filename.c_str(), (*bitmaps)[i],
                                           SkImageEncoder::kPNG_Type, 0);
            }
        }

        bitmaps->unref();
    }

    void report() {
        SkDebugf("%d Trans skipped (out of %d)\n", fTransSkipped, fTransTot);
        SkDebugf("%d Scales skipped (out of %d)\n", fScalesSkipped, fScalesTot);
    }

protected:
    int fTransSkipped;
    int fTransTot;

    int fScalesSkipped;
    int fScalesTot;

private:
    typedef SkPictureRecord INHERITED;
};

// Wrap SkPicture to allow installation of a SkFilterRecord object
class SkFilterPicture : public SkPicture {
public:
    SkFilterPicture(int width, int height, SkPictureRecord* record) {
        fWidth = width;
        fHeight = height;
        fRecord = record;
        SkSafeRef(fRecord);
    }

private:
    typedef SkPicture INHERITED;
};


// This function is not marked as 'static' so it can be referenced externally
// in the iOS build.
int tool_main(int argc, char** argv) {
    SkGraphics::Init();

    if (argc < 3) {
        usage();
        return -1;
    }

    SkString inFile, outFile, textureDir;

    char* const* stop = argv + argc;
    for (++argv; argv < stop; ++argv) {
        if (strcmp(*argv, "-i") == 0) {
            argv++;
            if (argv < stop && **argv) {
                inFile.set(*argv);
            } else {
                SkDebugf("missing arg for -i\n");
                usage();
                return -1;
            }
        } else if (strcmp(*argv, "-o") == 0) {
            argv++;
            if (argv < stop && **argv) {
                outFile.set(*argv);
            } else {
                SkDebugf("missing arg for -o\n");
                usage();
                return -1;
            }
        } else if (strcmp(*argv, "-t") == 0) {
            argv++;
            if (argv < stop && **argv) {
                textureDir.set(*argv);
            } else {
                SkDebugf("missing arg for -t\n");
                usage();
                return -1;
            }
        } else if (strcmp(*argv, "--help") == 0 || strcmp(*argv, "-h") == 0) {
            usage();
            return 0;
        } else {
            SkDebugf("unknown arg %s\n", *argv);
            usage();
            return -1;
        }
    }

    if (inFile.isEmpty()) {
        usage();
        return -1;
    }

    SkPicture* inPicture = NULL;

    SkFILEStream inStream(inFile.c_str());
    if (inStream.isValid()) {
        inPicture = SkNEW_ARGS(SkPicture, (&inStream));
    }

    if (NULL == inPicture) {
        SkDebugf("Could not read file %s\n", inFile.c_str());
        return -1;
    }

    SkBitmap bm;
    bm.setConfig(SkBitmap::kNo_Config, inPicture->width(), inPicture->height());
    SkAutoTUnref<SkDevice> dev(SkNEW_ARGS(SkDevice, (bm)));

    SkAutoTUnref<SkFilterRecord> filterRecord(SkNEW_ARGS(SkFilterRecord, (0, dev)));

    // Playback the read in picture to the SkFilterRecorder to allow filtering
    filterRecord->beginRecording();
    inPicture->draw(filterRecord);
    filterRecord->endRecording();

    filterRecord->report();

    if (!outFile.isEmpty()) {
        SkFilterPicture outPicture(inPicture->width(), inPicture->height(), filterRecord);
        SkFILEWStream outStream(outFile.c_str());

        outPicture.serialize(&outStream);
    }

    if (!textureDir.isEmpty()) {
        filterRecord->saveImages(textureDir);
    }

    SkGraphics::Term();

    return 0;
}

#if !defined SK_BUILD_FOR_IOS
int main(int argc, char * const argv[]) {
    return tool_main(argc, (char**) argv);
}
#endif
