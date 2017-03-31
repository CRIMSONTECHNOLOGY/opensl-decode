#include <assert.h>
#include <jni.h>
#include <string>
//#include <pthread.h>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>

#define LOG_TAG "OpenSLTest"
#define LOG_PRINT(LEVEL_, LOG_TAG_, ...) __android_log_print(LEVEL_, LOG_TAG_, __VA_ARGS__)
#define LOGD(...) LOG_PRINT(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define UNREFERENCED(x)  ((void)x)

#define xsl(x) {r = x; assert(r == SL_RESULT_SUCCESS);}

// rendering buffer
#define BLOCK_SAMPLES 1024
#define BLOCKS 2
SLint16 buffer[BLOCK_SAMPLES * BLOCKS * 2/*ch*/];

// OpenSL ES objects
SLEngineItf engineItf;
SLObjectItf engineObj;
SLObjectItf playerObj;
SLPlayItf playItf;
SLSeekItf seekItf;
SLAndroidSimpleBufferQueueItf bufferQueueItf;
SLPrefetchStatusItf prefetchStatusItf;
SLMetadataExtractionItf metadataExtractionItf;

// pthread signaling objects
pthread_cond_t cond;
pthread_mutex_t mutex;
SLuint32 prefetchStatus;
bool decodeFinished;

// for export
FILE *fpout;

// for metadata extraction
SLuint32 meta_channels;


void playInterfaceCallback(SLPlayItf caller, void *pContext, SLuint32 event) {
    UNREFERENCED(caller); UNREFERENCED(pContext);
    if (event & SL_PLAYEVENT_HEADATEND) {
        int ptr = ::pthread_mutex_lock(&mutex);
        assert(ptr==0);

        decodeFinished = true;

        ptr = ::pthread_cond_signal(&cond);
        assert(ptr==0);
        ptr = ::pthread_mutex_unlock(&mutex);
        assert(ptr==0);
    }
}

void bufferQueueCallback(SLAndroidSimpleBufferQueueItf caller, void *pContext) {
    UNREFERENCED(pContext);
    SLresult r;
    SLAndroidSimpleBufferQueueState state;

    xsl((*caller)->GetState(caller, &state));

    // write to the wave file
    int i = (state.index - 1) % BLOCKS;
    SLint16 *data = &buffer[i * BLOCK_SAMPLES * meta_channels];
    size_t samples = BLOCK_SAMPLES * meta_channels;
    size_t written = fwrite(data, sizeof(SLint16), samples, fpout);
    assert(written==samples);

    // enqueue next buffer
    xsl((*caller)->Enqueue(caller, data, sizeof(SLint16) * samples));
}

void prefetchStatusCallback(SLPrefetchStatusItf caller, void *pContext, SLuint32 event) {
    UNREFERENCED(pContext); UNREFERENCED(event);
    SLresult r;

    SLuint32 status = 0;

    xsl((*caller)->GetPrefetchStatus(caller, &status));

    int ptr = ::pthread_mutex_lock(&mutex);
    assert(ptr==0);

    prefetchStatus = status;

    ptr = ::pthread_cond_signal(&cond);
    assert(ptr==0);
    ptr = ::pthread_mutex_unlock(&mutex);
    assert(ptr==0);
}


void initialize() {
    LOGD("initialize()");
    SLresult r;

    xsl(::slCreateEngine(&engineObj, 0, nullptr, 0, nullptr, nullptr));
    xsl((*engineObj)->Realize(engineObj, SL_BOOLEAN_FALSE));
    xsl((*engineObj)->GetInterface(engineObj, SL_IID_ENGINE, &engineItf));

    playerObj = nullptr;
    playItf = nullptr;
    seekItf = nullptr;
    bufferQueueItf = nullptr;
    prefetchStatusItf = nullptr;
    metadataExtractionItf = nullptr;

    cond = {0};
    mutex = {0};
    prefetchStatus = 0;
    decodeFinished = false;
}

void setupPlayer(int fdin, off_t start, off_t length) {
    LOGD("setupPlayer()");
    SLresult r;

    // source locator
    SLDataLocator_AndroidFD locSrc = {SL_DATALOCATOR_ANDROIDFD, fdin, start, length};
    SLDataFormat_MIME mimeSrc = {SL_DATAFORMAT_MIME, nullptr, SL_CONTAINERTYPE_UNSPECIFIED};
    SLDataSource audioSrc = {&locSrc, &mimeSrc};

    // sink locator
    SLDataLocator_AndroidSimpleBufferQueue locSink = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, BLOCKS};
    SLDataFormat_PCM pcmSink;
    pcmSink.formatType = SL_DATAFORMAT_PCM;
    pcmSink.numChannels = 2;
    pcmSink.samplesPerSec = SL_SAMPLINGRATE_44_1;
    pcmSink.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
    pcmSink.containerSize = 16;
    pcmSink.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    pcmSink.endianness = SL_BYTEORDER_LITTLEENDIAN;
    SLDataSink audioSink = {&locSink, &pcmSink};

    // create playerObj
    const SLInterfaceID ids[] = {SL_IID_SEEK, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_PREFETCHSTATUS, SL_IID_METADATAEXTRACTION};
    const SLboolean req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    xsl((*engineItf)->CreateAudioPlayer(engineItf, &playerObj, &audioSrc, &audioSink, sizeof(ids) / sizeof(SLInterfaceID), ids, req));
    xsl((*playerObj)->Realize(playerObj, SL_BOOLEAN_FALSE));
    // get metadataExtractionItf
    xsl((*playerObj)->GetInterface(playerObj, SL_IID_METADATAEXTRACTION, &metadataExtractionItf));
    // get seekItf
    xsl((*playerObj)->GetInterface(playerObj, SL_IID_SEEK, &seekItf));
    // playItf
    xsl((*playerObj)->GetInterface(playerObj, SL_IID_PLAY, &playItf));
    xsl((*playItf)->SetCallbackEventsMask(playItf, SL_PLAYEVENT_HEADATEND));
    xsl((*playItf)->RegisterCallback(playItf, playInterfaceCallback, nullptr));
    // bufferQueueItf
    xsl((*playerObj)->GetInterface(playerObj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &bufferQueueItf));
    xsl((*bufferQueueItf)->RegisterCallback(bufferQueueItf, bufferQueueCallback, nullptr));
    // prefetchStatusItf
    xsl((*playerObj)->GetInterface(playerObj, SL_IID_PREFETCHSTATUS, &prefetchStatusItf));
    xsl((*prefetchStatusItf)->SetCallbackEventsMask(prefetchStatusItf, SL_PREFETCHEVENT_STATUSCHANGE | SL_PREFETCHEVENT_FILLLEVELCHANGE));
    xsl((*prefetchStatusItf)->RegisterCallback(prefetchStatusItf, prefetchStatusCallback, nullptr));
}

void prefetch() {
    LOGD("prefetch()");
    SLresult r;

    // prefetch
    prefetchStatus = 0;

    xsl((*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PAUSED));

    int ptr = ::pthread_mutex_lock(&mutex);
    assert(ptr==0);
    while (prefetchStatus == 0) {
        ptr = ::pthread_cond_wait(&cond, &mutex);
        assert(ptr==0);
    }
    ptr = ::pthread_mutex_unlock(&mutex);
    assert(ptr==0);

    // extract metadata
    int8_t keyBuf_[128], valueBuf_[128];
    SLMetadataInfo *keyBuf = reinterpret_cast<SLMetadataInfo*>(keyBuf_);
    SLMetadataInfo *valueBuf = reinterpret_cast<SLMetadataInfo*>(valueBuf_);
    const char *key = (const char *)(keyBuf->data);
    const SLuint32 *value = (const SLuint32 *)(valueBuf->data);
    SLuint32 itemCount = 0;

    xsl((*metadataExtractionItf)->GetItemCount(metadataExtractionItf, &itemCount));

    for (SLuint32 i = 0; i < itemCount; ++i) {
        SLuint32 keySize, valueSize;

        xsl((*metadataExtractionItf)->GetKeySize(metadataExtractionItf, i, &keySize));
        xsl((*metadataExtractionItf)->GetValueSize(metadataExtractionItf, i, &valueSize));
        if (keySize > 128 || valueSize > 128) {
            continue;
        }
        xsl((*metadataExtractionItf)->GetKey(metadataExtractionItf, i, keySize, keyBuf));
        xsl((*metadataExtractionItf)->GetValue(metadataExtractionItf, i, valueSize, valueBuf));

        if (valueBuf->size != sizeof(SLuint32)) {
            continue;
        }

        if (::strncmp(key, ANDROID_KEY_PCMFORMAT_NUMCHANNELS, keySize) == 0) {
            meta_channels = *value;
        } else if (::strncmp(key, ANDROID_KEY_PCMFORMAT_BITSPERSAMPLE, keySize) == 0) {
            assert(*value==SL_PCMSAMPLEFORMAT_FIXED_16);
        } else if (::strncmp(key, ANDROID_KEY_PCMFORMAT_CONTAINERSIZE, keySize) == 0) {
            assert(*value==16);
        } else if (::strncmp(key, ANDROID_KEY_PCMFORMAT_ENDIANNESS, keySize) == 0) {
            assert(*value==SL_BYTEORDER_LITTLEENDIAN);
        }
    }
}

void seek() {
    LOGD("seek()");
    SLresult r;
    xsl((*seekItf)->SetPosition(seekItf, 0, SL_SEEKMODE_ACCURATE));
}

void decode() {
    LOGD("decode()");
    SLresult r;

    for (int i = 0; i < BLOCKS; i++) {
        xsl((*bufferQueueItf)->Enqueue(bufferQueueItf, &buffer[i * BLOCK_SAMPLES * meta_channels], sizeof(SLint16) * BLOCK_SAMPLES * meta_channels));
    }

    decodeFinished = false;

    xsl((*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING));

    int ptr = ::pthread_mutex_lock(&mutex);
    assert(ptr==0);
    while (!decodeFinished) {
        ptr = ::pthread_cond_wait(&cond, &mutex);
        assert(ptr==0);
    }
    ptr = ::pthread_mutex_unlock(&mutex);
    assert(ptr==0);

    xsl((*playItf)->SetPlayState(playItf, SL_PLAYSTATE_STOPPED));
}

extern "C"
jboolean Java_jp_crimsontech_opensltest_MainActivity_toWavFile(
        JNIEnv *env, jclass clazz, jobject assetManager, jstring filename, jstring exportPath, jboolean execSeek) {
    UNREFERENCED(clazz);
    off_t start, length;
    const char *cFName = env->GetStringUTFChars(filename, nullptr);
    const char *cExportPath = env->GetStringUTFChars(exportPath, nullptr);

    AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
    assert(mgr!=nullptr);

    AAsset *asset = AAssetManager_open(mgr, cFName, AASSET_MODE_UNKNOWN);
    assert(asset!=nullptr);

    int fdin = AAsset_openFileDescriptor(asset, &start, &length);
    assert(fdin>=0);

    env->ReleaseStringUTFChars(filename, cFName);

    AAsset_close(asset);

    fpout = fopen(cExportPath, "wb");
    assert(fpout!=nullptr);

    env->ReleaseStringUTFChars(exportPath, cExportPath);

    // convert a content file to a wave file
    initialize();
    setupPlayer(fdin, start, length);
    prefetch();
    if (execSeek == JNI_TRUE) {
        seek();
    }
    decode();

    // write a wave file header
    int32_t bytes = (int32_t)ftell(fpout);
    fseek(fpout, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, fpout);
    int32_t temp32 = bytes - 8;
    fwrite(&temp32, 4, 1, fpout); // file size
    fwrite("WAVEfmt \x10\x00\x00\x00\x01\x00", 1, 14, fpout);
    int16_t temp16 = (int16_t)meta_channels;
    fwrite(&temp16, 2, 1, fpout); // channels
    temp32 = 44100;
    fwrite(&temp32, 4, 1, fpout); // samples per sec
    temp32 *= meta_channels * 2/*(16bit)*/;
    fwrite(&temp32, 4, 1, fpout); // bytes per sec
    temp16 = (int16_t)(meta_channels * 2/*(16bit)*/);
    fwrite(&temp16, 2, 1, fpout); // bytes per sample frame
    fwrite("\x10\x00", 1, 2, fpout);
    fwrite("data", 1, 4, fpout);
    temp32 = bytes - 44;
    fwrite(&temp32, 4, 1, fpout); // data chunk size
    fclose(fpout);

    return JNI_TRUE;
}
