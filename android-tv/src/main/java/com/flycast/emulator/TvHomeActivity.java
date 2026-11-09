package com.flycast.emulator;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.StateListDrawable;
import android.net.Uri;
import android.opengl.EGL14;
import android.opengl.EGLConfig;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.opengl.EGLSurface;
import android.opengl.GLES20;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

/** TV-first launcher and real Flycast settings front-end. */
public final class TvHomeActivity extends Activity {
    private static final int REQUEST_PICK_GAME = 4101;
    private static final String PREFS = "dreamcast_tv_home";
    private static final int MAX_RECENTS = 5;
    private static final int[] RENDER_HEIGHTS = {480, 720, 960, 1200, 1440, 1800};
    private static final String[] FILTER_NAMES = {
            "Выключен", "CRT 90-х", "Старый телевизор",
            "Composite", "RGB-маска", "VHS"
    };

    private final int accent = Color.rgb(255, 190, 48);
    private final int foreground = Color.rgb(242, 245, 250);
    private final int muted = Color.rgb(160, 172, 190);
    private int maxRenderTarget = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        maxRenderTarget = queryMaxRenderTarget();
        Intent launchIntent = getIntent();
        if (launchIntent != null && Intent.ACTION_VIEW.equals(launchIntent.getAction())
                && launchIntent.getData() != null) {
            Uri uri = launchIntent.getData();
            remember(uri, queryDisplayName(uri));
            launchCore(uri);
            finish();
            return;
        }

        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
        buildHome();
    }

    private void buildHome() {
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setBackgroundColor(Color.rgb(8, 12, 21));

        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setGravity(Gravity.CENTER_HORIZONTAL);
        page.setPadding(dp(56), dp(36), dp(56), dp(36));

        TextView eyebrow = new TextView(this);
        eyebrow.setText("ANDROID TV  •  SEGA DREAMCAST");
        eyebrow.setTextColor(accent);
        eyebrow.setTextSize(14);
        eyebrow.setLetterSpacing(0.12f);
        eyebrow.setTypeface(Typeface.DEFAULT_BOLD);
        page.addView(eyebrow, fullWidthWrap());

        TextView title = new TextView(this);
        title.setText("DREAMCAST");
        title.setTextColor(foreground);
        title.setTextSize(42);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setPadding(0, dp(8), 0, 0);
        page.addView(title, fullWidthWrap());

        TextView subtitle = new TextView(this);
        subtitle.setText("Эмулятор для большого экрана");
        subtitle.setTextColor(muted);
        subtitle.setTextSize(19);
        subtitle.setPadding(0, dp(2), 0, dp(24));
        page.addView(subtitle, fullWidthWrap());

        addAction(page, "ВЫБРАТЬ ИГРУ",
                "Открыть образ игры из памяти приставки или USB-накопителя",
                this::pickGame, true);

        List<RecentGame> recents = readRecents();
        if (!recents.isEmpty()) {
            addSectionTitle(page, "НЕДАВНИЕ ИГРЫ");
            for (RecentGame game : recents) {
                addAction(page, game.name, "Запустить игру",
                        () -> launchCore(Uri.parse(game.uri)), false);
            }
        }

        addSectionTitle(page, "ГРАФИКА");
        addGraphicsSettings(page);

        addSectionTitle(page, "БИБЛИОТЕКА И СОХРАНЕНИЯ");
        addAction(page, "Открыть Flycast",
                "Встроенная библиотека, добавление папок, настройки ядра, VMU и состояния сохранения",
                this::openCoreBrowser, false);

        TextView footer = new TextView(this);
        footer.setText("Игры и BIOS не входят в приложение. Используйте собственные файлы. "
                + "Настройки графики применяются к ядру Flycast при запуске.");
        footer.setTextColor(muted);
        footer.setTextSize(13);
        footer.setGravity(Gravity.CENTER);
        footer.setPadding(0, dp(28), 0, dp(8));
        page.addView(footer, fullWidthWrap());

        scroll.addView(page);
        setContentView(scroll);
        page.setFocusableInTouchMode(true);
        page.requestFocus();
    }

    private void addGraphicsSettings(LinearLayout page) {
        SharedPreferences prefs = getSharedPreferences(PREFS, MODE_PRIVATE);
        int profile = clamp(prefs.getInt("tv_graphics_profile", 1), 0, 2);
        int renderHeight = supportedRenderHeight(prefs.getInt("tv_render_height", 720));
        int filter = clamp(prefs.getInt("tv_retro_filter", 0), 0, FILTER_NAMES.length - 1);

        addAction(page, (profile == 0 ? "✓ " : "") + "ПРОИЗВОДИТЕЛЬНОСТЬ",
                "480p, адаптивный пропуск кадров при нагрузке, лёгкая фильтрация текстур",
                () -> selectProfile(0), profile == 0);
        addAction(page, (profile == 1 ? "✓ " : "") + "БАЛАНС",
                "720p, адаптивный пропуск кадров, умеренная фильтрация текстур",
                () -> selectProfile(1), profile == 1);
        addAction(page, (profile == 2 ? "✓ " : "") + "КАЧЕСТВО",
                "960p, без автоматического пропуска кадров, более чёткие наклонные текстуры",
                () -> selectProfile(2), profile == 2);

        addSectionTitle(page, "ВНУТРЕННЕЕ РАЗРЕШЕНИЕ");
        for (int height : RENDER_HEIGHTS) {
            if (!isRenderHeightSupported(height)) continue;
            final int selectedHeight = height;
            String detail = height == 480 ? "Стандартное разрешение Dreamcast"
                    : "Внутренний рендер " + height + "p; выше чёткость, выше нагрузка на GPU";
            addAction(page, (renderHeight == height ? "✓ " : "") + height + "p",
                    detail, () -> selectRenderHeight(selectedHeight), renderHeight == height);
        }

        addSectionTitle(page, "РЕТРО-ФИЛЬТРЫ · ОДИН ОДНОВРЕМЕННО");
        for (int i = 0; i < FILTER_NAMES.length; i++) {
            final int selectedFilter = i;
            String detail;
            switch (i) {
                case 1: detail = "Лёгкие электронно-лучевые строки и затемнение по краям"; break;
                case 2: detail = "Мягкая картинка и горизонтальная развертка старого ТВ"; break;
                case 3: detail = "Мягкость и лёгкое цветовое смешение композитного сигнала"; break;
                case 4: detail = "RGB-субпиксельная маска экрана"; break;
                case 5: detail = "Лёгкое цветовое смещение и горизонтальная нестабильность VHS"; break;
                default: detail = "Чистое изображение без дополнительной обработки"; break;
            }
            addAction(page, (filter == i ? "✓ " : "") + FILTER_NAMES[i],
                    detail, () -> selectRetroFilter(selectedFilter), filter == i);
        }

        TextView note = new TextView(this);
        note.setText("Сглаживание достигается повышением внутреннего разрешения (supersampling). "
                + "1800p показывается только тогда, когда графический драйвер сообщает подходящий "
                + "размер текстуры и буфера. Даже при поддержке драйвером такой режим может быть "
                + "тяжёлым для RK3528; при рывках выберите 720p или 960p.");
        note.setTextColor(muted);
        note.setTextSize(13);
        note.setPadding(0, dp(8), 0, dp(14));
        page.addView(note, fullWidthWrap());
    }

    private void addSectionTitle(LinearLayout page, String label) {
        TextView heading = new TextView(this);
        heading.setText(label);
        heading.setTextColor(accent);
        heading.setTextSize(13);
        heading.setTypeface(Typeface.DEFAULT_BOLD);
        heading.setLetterSpacing(0.1f);
        heading.setPadding(0, dp(22), 0, dp(8));
        page.addView(heading, fullWidthWrap());
    }

    private void addAction(LinearLayout parent, String title, String description,
                           Runnable action, boolean primary) {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setGravity(Gravity.CENTER_VERTICAL);
        card.setPadding(dp(22), dp(14), dp(22), dp(14));
        card.setFocusable(true);
        card.setClickable(true);
        card.setBackground(actionBackground(primary));
        card.setOnClickListener(v -> action.run());

        TextView heading = new TextView(this);
        heading.setText(title);
        heading.setTextColor(primary ? Color.rgb(18, 22, 30) : foreground);
        heading.setTextSize(21);
        heading.setTypeface(Typeface.DEFAULT_BOLD);
        card.addView(heading, fullWidthWrap());

        TextView detail = new TextView(this);
        detail.setText(description);
        detail.setTextColor(primary ? Color.rgb(55, 53, 48) : muted);
        detail.setTextSize(14);
        detail.setPadding(0, dp(4), 0, 0);
        card.addView(detail, fullWidthWrap());

        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.bottomMargin = dp(9);
        parent.addView(card, params);
    }

    private StateListDrawable actionBackground(boolean primary) {
        int normal = primary ? accent : Color.rgb(22, 31, 46);
        int focused = primary ? Color.rgb(255, 211, 105) : Color.rgb(42, 57, 78);
        int stroke = primary ? Color.rgb(255, 223, 150) : Color.rgb(65, 83, 110);
        StateListDrawable states = new StateListDrawable();
        states.addState(new int[]{android.R.attr.state_focused}, rounded(focused, stroke));
        states.addState(new int[]{android.R.attr.state_pressed}, rounded(focused, stroke));
        states.addState(new int[]{}, rounded(normal, primary ? 0 : Color.rgb(42, 54, 73)));
        return states;
    }

    private GradientDrawable rounded(int color, int stroke) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(color);
        drawable.setCornerRadius(dp(12));
        if (stroke != 0) drawable.setStroke(dp(2), stroke);
        return drawable;
    }

    private void selectProfile(int profile) {
        int resolution = profile == 0 ? 480 : profile == 1 ? 720 : 960;
        if (!isRenderHeightSupported(resolution)) resolution = 480;
        getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                .putInt("tv_graphics_profile", profile)
                .putInt("tv_render_height", resolution)
                .apply();
        buildHome();
    }

    private void selectRenderHeight(int height) {
        if (!isRenderHeightSupported(height)) return;
        getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                .putInt("tv_render_height", height)
                .apply();
        buildHome();
    }

    private void selectRetroFilter(int filter) {
        getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                .putInt("tv_retro_filter", clamp(filter, 0, FILTER_NAMES.length - 1))
                .apply();
        buildHome();
    }

    private boolean isRenderHeightSupported(int height) {
        if (maxRenderTarget <= 0) return height <= 720;
        int requiredWidth = (int) Math.ceil(height * 16.0 / 9.0);
        return height <= maxRenderTarget && requiredWidth <= maxRenderTarget;
    }

    private int supportedRenderHeight(int requested) {
        for (int height : RENDER_HEIGHTS) {
            if (height == requested && isRenderHeightSupported(height)) return height;
        }
        return 480;
    }

    private int queryMaxRenderTarget() {
        EGLDisplay display = EGL14.EGL_NO_DISPLAY;
        EGLContext context = EGL14.EGL_NO_CONTEXT;
        EGLSurface surface = EGL14.EGL_NO_SURFACE;
        boolean initialized = false;
        try {
            display = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
            if (display == EGL14.EGL_NO_DISPLAY) return 0;
            int[] versions = new int[2];
            if (!EGL14.eglInitialize(display, versions, 0, versions, 1)) return 0;
            initialized = true;

            int[] configAttrs = {
                    EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT,
                    EGL14.EGL_RED_SIZE, 8, EGL14.EGL_GREEN_SIZE, 8,
                    EGL14.EGL_BLUE_SIZE, 8, EGL14.EGL_ALPHA_SIZE, 8,
                    EGL14.EGL_NONE
            };
            EGLConfig[] configs = new EGLConfig[1];
            int[] count = new int[1];
            if (!EGL14.eglChooseConfig(display, configAttrs, 0, configs, 0, 1, count, 0)
                    || count[0] == 0) return 0;

            int[] contextAttrs = {EGL14.EGL_CONTEXT_CLIENT_VERSION, 2, EGL14.EGL_NONE};
            context = EGL14.eglCreateContext(display, configs[0], EGL14.EGL_NO_CONTEXT,
                    contextAttrs, 0);
            if (context == EGL14.EGL_NO_CONTEXT) return 0;

            int[] surfaceAttrs = {
                    EGL14.EGL_WIDTH, 1, EGL14.EGL_HEIGHT, 1, EGL14.EGL_NONE
            };
            surface = EGL14.eglCreatePbufferSurface(display, configs[0], surfaceAttrs, 0);
            if (surface == EGL14.EGL_NO_SURFACE) return 0;
            if (!EGL14.eglMakeCurrent(display, surface, surface, context)) return 0;

            int[] textureLimit = new int[1];
            int[] renderbufferLimit = new int[1];
            GLES20.glGetIntegerv(GLES20.GL_MAX_TEXTURE_SIZE, textureLimit, 0);
            GLES20.glGetIntegerv(GLES20.GL_MAX_RENDERBUFFER_SIZE, renderbufferLimit, 0);
            return Math.min(textureLimit[0], renderbufferLimit[0]);
        } catch (Throwable ignored) {
            return 0;
        } finally {
            if (display != EGL14.EGL_NO_DISPLAY) {
                EGL14.eglMakeCurrent(display, EGL14.EGL_NO_SURFACE,
                        EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT);
                if (surface != EGL14.EGL_NO_SURFACE)
                    EGL14.eglDestroySurface(display, surface);
                if (context != EGL14.EGL_NO_CONTEXT)
                    EGL14.eglDestroyContext(display, context);
                if (initialized) EGL14.eglTerminate(display);
            }
        }
    }

    private static int clamp(int value, int min, int max) {
        return Math.max(min, Math.min(max, value));
    }

    private void pickGame() {
        Intent picker = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        picker.addCategory(Intent.CATEGORY_OPENABLE);
        picker.setType("*/*");
        picker.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        startActivityForResult(picker, REQUEST_PICK_GAME);
    }

    private void openCoreBrowser() {
        startActivity(new Intent(this, NativeGLActivity.class));
    }

    private void launchCore(Uri uri) {
        if (uri == null) return;
        Intent core = new Intent(this, NativeGLActivity.class);
        core.setAction(Intent.ACTION_VIEW);
        core.setDataAndType(uri, "*/*");
        core.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        startActivity(core);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQUEST_PICK_GAME || resultCode != RESULT_OK
                || data == null || data.getData() == null) return;

        Uri uri = data.getData();
        try {
            int readFlag = data.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION;
            if (readFlag != 0) getContentResolver().takePersistableUriPermission(uri, readFlag);
        } catch (SecurityException ignored) {
            // Temporary provider grants still allow this launch; the URI may not persist.
        }
        remember(uri, queryDisplayName(uri));
        launchCore(uri);
    }

    private String queryDisplayName(Uri uri) {
        Cursor cursor = null;
        try {
            cursor = getContentResolver().query(uri,
                    new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null);
            if (cursor != null && cursor.moveToFirst()) {
                int index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (index >= 0) {
                    String value = cursor.getString(index);
                    if (value != null && !value.trim().isEmpty()) return value;
                }
            }
        } catch (Exception ignored) {
        } finally {
            if (cursor != null) cursor.close();
        }
        String last = uri.getLastPathSegment();
        return last == null ? "Игра Dreamcast" : last;
    }

    private void remember(Uri uri, String name) {
        if (uri == null) return;
        List<RecentGame> oldItems = readRecents();
        List<RecentGame> items = new ArrayList<>();
        items.add(new RecentGame(uri.toString(), name == null ? "Игра Dreamcast" : name));
        for (RecentGame item : oldItems) {
            if (!item.uri.equals(uri.toString()) && items.size() < MAX_RECENTS) items.add(item);
        }

        SharedPreferences.Editor editor = getSharedPreferences(PREFS, MODE_PRIVATE).edit();
        for (int i = 0; i < MAX_RECENTS; i++) {
            if (i < items.size()) {
                editor.putString("uri_" + i, items.get(i).uri);
                editor.putString("name_" + i, items.get(i).name);
            } else {
                editor.remove("uri_" + i);
                editor.remove("name_" + i);
            }
        }
        editor.apply();
    }

    private List<RecentGame> readRecents() {
        SharedPreferences prefs = getSharedPreferences(PREFS, MODE_PRIVATE);
        List<RecentGame> items = new ArrayList<>();
        for (int i = 0; i < MAX_RECENTS; i++) {
            String uri = prefs.getString("uri_" + i, null);
            if (uri != null) items.add(new RecentGame(uri,
                    prefs.getString("name_" + i, "Игра Dreamcast")));
        }
        return items;
    }

    private LinearLayout.LayoutParams fullWidthWrap() {
        return new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    private int dp(float value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private static final class RecentGame {
        final String uri;
        final String name;
        RecentGame(String uri, String name) {
            this.uri = uri;
            this.name = name;
        }
    }
}
