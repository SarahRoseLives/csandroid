package com.cspsp.android;

import android.media.AudioAttributes;
import android.media.MediaPlayer;
import android.media.SoundPool;
import android.util.Log;

/**
 * Minimal audio bridge used by the JGE JSoundSystem Android backend.
 * Sound effects are played through SoundPool; background music is
 * streamed through a single MediaPlayer.
 *
 * All methods are called from native code via JNI on the game thread.
 */
public final class AudioBridge {

    private static final String TAG = "CSPSP.AudioBridge";

    private static SoundPool sSoundPool;
    private static MediaPlayer sMusicPlayer;
    private static int sMusicVolume = 255;

    public static int init() {
        if (sSoundPool == null) {
            AudioAttributes attrs = new AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_GAME)
                    .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                    .build();
            sSoundPool = new SoundPool.Builder()
                    .setMaxStreams(16)
                    .setAudioAttributes(attrs)
                    .build();
        }
        return 0;
    }

    public static int loadSample(String path) {
        if (sSoundPool == null) init();
        try {
            return sSoundPool.load(path, 1);
        } catch (Throwable t) {
            Log.e(TAG, "loadSample failed: " + path, t);
            return -1;
        }
    }

    public static int playSample(int sampleId, int volume, int panning) {
        if (sSoundPool == null || sampleId < 0) return -1;
        float left = volume / 255.0f;
        float right = volume / 255.0f;
        if (panning < 127) {
            right *= panning / 127.0f;
        } else if (panning > 127) {
            left *= (255 - panning) / 128.0f;
        }
        try {
            return sSoundPool.play(sampleId, left, right, 1, 0, 1.0f);
        } catch (Throwable t) {
            Log.e(TAG, "playSample failed", t);
            return -1;
        }
    }

    public static int stopSample(int streamId) {
        if (sSoundPool == null || streamId < 0) return -1;
        try {
            sSoundPool.stop(streamId);
            return 1;
        } catch (Throwable t) {
            return -1;
        }
    }

    public static int loadMusic(String path) {
        stopMusic();
        try {
            sMusicPlayer = new MediaPlayer();
            sMusicPlayer.setDataSource(path);
            sMusicPlayer.setAudioAttributes(
                    new AudioAttributes.Builder()
                            .setUsage(AudioAttributes.USAGE_MEDIA)
                            .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                            .build());
            sMusicPlayer.prepare();
            setVolume(sMusicVolume);
            return 1;
        } catch (Throwable t) {
            Log.e(TAG, "loadMusic failed: " + path, t);
            sMusicPlayer = null;
            return -1;
        }
    }

    public static int playMusic(boolean loop) {
        if (sMusicPlayer == null) return -1;
        try {
            sMusicPlayer.setLooping(loop);
            sMusicPlayer.start();
            return 1;
        } catch (Throwable t) {
            Log.e(TAG, "playMusic failed", t);
            return -1;
        }
    }

    public static int stopMusic() {
        if (sMusicPlayer == null) return -1;
        try {
            if (sMusicPlayer.isPlaying()) sMusicPlayer.stop();
            sMusicPlayer.release();
        } catch (Throwable ignored) {
        }
        sMusicPlayer = null;
        return 1;
    }

    public static int setVolume(int volume) {
        sMusicVolume = Math.max(0, Math.min(255, volume));
        if (sMusicPlayer != null) {
            float v = sMusicVolume / 255.0f;
            sMusicPlayer.setVolume(v, v);
        }
        return 1;
    }
}
