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
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.provider.OpenableColumns;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.documentfile.provider.DocumentFile;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;
import android.opengl.GLES20;

/** Russian Android-TV shell for Flycast. All graphics choices are written to Flycast emu.cfg. */
public final class TvHomeActivity extends Activity {
    private static final int REQUEST_PICK_GAME = 4101;
    private static final int REQUEST_PICK_FOLDER = 4102;
    private static final int REQUEST_PICK_COVER = 4103;
    private static final int MAX_LIBRARY = 500;
    private static final int MAX_SCAN_DEPTH = 8;
    private static final int MAX_SCAN_FILES = 5000;
    private static final String PREFS = "dreamcast_tv_home";
    private static final String DEFAULT_PROFILE = "balanced";

    private final int accent = Color.rgb(255, 190, 48);
    private final int foreground = Color.rgb(242, 245, 250);
    private final int muted = Color.rgb(160, 172, 190);
    private int maxGlSize;
    private String coverTargetUri;

    private SharedPreferences homePrefs() {
        return getSharedPreferences(PREFS, MODE_PRIVATE);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Intent launchIntent = getIntent();
        if (launchIntent != null && Intent.ACTION_VIEW.equals(launchIntent.getAction())
                && launchIntent.getData() != null) {
            Uri uri = launchIntent.getData();
            rememberGame(uri, queryDisplayName(uri));
            launchCore(uri);
            finish();
            return;
        }
        maxGlSize = queryMaxGlSize();
        hideSystemBars();
        buildHome();
    }

    private void hideSystemBars() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
    }

    private int queryMaxGlSize() {
        EGL10 egl = null;
        EGLDisplay display = null;
        EGLSurface surface = null;
        EGLContext context = null;
        try {
            egl = (EGL10) EGLContext.getEGL();
            display = egl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
            if (display == EGL10.EGL_NO_DISPLAY) return 0;
            int[] version = new int[2];
            if (!egl.eglInitialize(display, version)) return 0;

            final int EGL_RENDERABLE_TYPE = 0x3040;
            final int EGL_OPENGL_ES2_BIT = 4;
            final int EGL_SURFACE_TYPE = 0x3033;
            final int EGL_PBUFFER_BIT = 1;
            int[] configAttrs = {
                    EGL10.EGL_RED_SIZE, 8, EGL10.EGL_GREEN_SIZE, 8, EGL10.EGL_BLUE_SIZE, 8,
                    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL10.EGL_NONE
            };
            EGLConfig[] configs = new EGLConfig[1];
            int[] count = new int[1];
            if (!egl.eglChooseConfig(display, configAttrs, configs, 1, count) || count[0] < 1) return 0;
            int[] contextAttrs = {0x3098, 2, EGL10.EGL_NONE};
            context = egl.eglCreateContext(display, configs[0], EGL10.EGL_NO_CONTEXT, contextAttrs);
            if (context == EGL10.EGL_NO_CONTEXT) return 0;
            int[] surfaceAttrs = {0x3057, 1, 0x3056, 1, EGL10.EGL_NONE};
            surface = egl.eglCreatePbufferSurface(display, configs[0], surfaceAttrs);
            if (surface == EGL10.EGL_NO_SURFACE) return 0;
            if (!egl.eglMakeCurrent(display, surface, surface, context)) return 0;
            int[] texture = new int[1];
            int[] renderbuffer = new int[1];
            GLES20.glGetIntegerv(GLES20.GL_MAX_TEXTURE_SIZE, texture, 0);
            GLES20.glGetIntegerv(0x84E8, renderbuffer, 0); // GL_MAX_RENDERBUFFER_SIZE
            int result = texture[0];
            if (renderbuffer[0] > 0) result = Math.min(result, renderbuffer[0]);
            return result > 0 ? result : 0;
        } catch (Throwable ignored) {
            return 0;
        } finally {
            if (egl != null && display != null && display != EGL10.EGL_NO_DISPLAY) {
                try {
                    egl.eglMakeCurrent(display, EGL10.EGL_NO_SURFACE,
                            EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_CONTEXT);
                    if (surface != null && surface != EGL10.EGL_NO_SURFACE)
                        egl.eglDestroySurface(display, surface);
                    if (context != null && context != EGL10.EGL_NO_CONTEXT)
                        egl.eglDestroyContext(display, context);
                    egl.eglTerminate(display);
                } catch (Throwable ignored) { }
            }
        }
    }

    private boolean supportsResolution(int height) {
        if (height > 1800) return false;
        if (maxGlSize <= 0) return height <= 1440;
        int width16x9 = (height * 16 + 8) / 9;
        return width16x9 <= maxGlSize;
    }

    private void buildHome() {
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setBackgroundColor(Color.rgb(8, 12, 21));
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setGravity(Gravity.CENTER_HORIZONTAL);
        page.setPadding(dp(46), dp(30), dp(46), dp(34));

        addEyebrow(page, "ANDROID TV  •  SEGA DREAMCAST");
        addTitle(page, "DREAMCAST", 40);
        addSubtitle(page, "Эмулятор для большого экрана");

        addAction(page, "БИБЛИОТЕКА ИГР",
                "Открыть список добавленных игр, выбрать игру или обложку",
                this::buildLibraryPage, true);
        addAction(page, "СКАНИРОВАТЬ ПАПКУ С ИГРАМИ",
                "Найти игры Dreamcast во внутренней памяти или на USB",
                this::pickGameFolder, false);
        addAction(page, "ДОБАВИТЬ ИГРУ",
                "Выбрать отдельный GDI, CHD, CDI, CUE или архив",
                this::pickGame, false);
        addAction(page, "ГРАФИКА И СГЛАЖИВАНИЕ",
                "Профили производительности, разрешение до 1800p и ретро-фильтры",
                this::buildSettingsPage, false);
        addAction(page, "ИНТЕРФЕЙС FLYCAST",
                "Встроенный браузер игр, настройки BIOS, управления, сохранений и VMU",
                this::openCoreBrowser, false);

        List<GameItem> games = readLibrary();
        TextView status = new TextView(this);
        String gpuInfo = maxGlSize > 0
                ? "Проверенный предел GL: " + maxGlSize + " px"
                : "Предел GL будет дополнительно проверен драйвером при запуске";
        status.setText(games.size() + " игр в библиотеке  •  " + gpuInfo);
        status.setTextColor(muted);
        status.setTextSize(13);
        status.setPadding(0, dp(18), 0, 0);
        page.addView(status, fullWidthWrap());

        addFooter(page);
        scroll.addView(page);
        setContentView(scroll);
        focusFirst(page);
    }

    private void buildLibraryPage() {
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setBackgroundColor(Color.rgb(8, 12, 21));
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(dp(46), dp(26), dp(46), dp(34));

        addEyebrow(page, "ТВОЯ КОЛЛЕКЦИЯ");
        addTitle(page, "БИБЛИОТЕКА ИГР", 32);
        addSubtitle(page, "Игры запускаются непосредственно в оригинальном ядре Flycast.");

        addAction(page, "＋ ДОБАВИТЬ ИГРУ", "Выбрать отдельный файл игры", this::pickGame, true);
        addAction(page, "СКАНИРОВАТЬ ПАПКУ", "Добавить совместимые игры из папки или USB",
                this::pickGameFolder, false);

        List<GameItem> games = readLibrary();
        if (games.isEmpty()) {
            addSubtitle(page, "Пока нет игр. Добавь файлы или выбери папку с играми.");
        } else {
            TextView heading = sectionHeading("ИГРЫ (" + games.size() + ")");
            page.addView(heading, fullWidthWrap());
            for (GameItem game : games) addGameCard(page, game);
        }

        addAction(page, "← НА ГЛАВНЫЙ ЭКРАН", "Вернуться в меню", this::buildHome, false);
        scroll.addView(page);
        setContentView(scroll);
        focusFirst(page);
    }

    private void buildSettingsPage() {
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setBackgroundColor(Color.rgb(8, 12, 21));
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(dp(46), dp(26), dp(46), dp(34));

        addEyebrow(page, "НАСТРОЙКИ ВИДЕО");
        addTitle(page, "ГРАФИКА", 34);
        addSubtitle(page, "Параметры записываются в emu.cfg и применяются к настоящему ядру Flycast.");

        String profile = homePrefs().getString("profile", DEFAULT_PROFILE);
        TextView profileHeading = sectionHeading("ПРОФИЛЬ ПРОИЗВОДИТЕЛЬНОСТИ");
        page.addView(profileHeading, fullWidthWrap());
        addAction(page, profile.equals("performance") ? "✓ ПРОИЗВОДИТЕЛЬНОСТЬ" : "ПРОИЗВОДИТЕЛЬНОСТЬ",
                "480p • минимальная нагрузка на GPU и память",
                () -> chooseProfile("performance"), false);
        addAction(page, profile.equals("balanced") ? "✓ БАЛАНС" : "БАЛАНС",
                "720p • рекомендуемый стартовый профиль",
                () -> chooseProfile("balanced"), false);
        addAction(page, profile.equals("quality") ? "✓ КАЧЕСТВО" : "КАЧЕСТВО",
                "1440p • высокая нагрузка; использовать при стабильной работе",
                () -> chooseProfile("quality"), false);

        TextView resolutionHeading = sectionHeading("ВНУТРЕННЕЕ РАЗРЕШЕНИЕ / СУПЕРСЭМПЛИНГ");
        page.addView(resolutionHeading, fullWidthWrap());
        addSubtitle(page, "Это разрешение рендеринга внутри эмулятора, а не режим HDMI-телевизора. Повышение разрешения даёт сглаживание методом supersampling, но увеличивает нагрузку.");
        int selectedResolution = homePrefs().getInt("resolution", profileResolution(profile));
        int[] resolutions = {480, 720, 960, 1200, 1440, 1800};
        for (int value : resolutions) {
            boolean supported = supportsResolution(value);
            String label = (value == selectedResolution ? "✓ " : "")
                    + value + "p" + (value == 480 ? " • native" : " • supersampling");
            if (value == 1800 && !supported) label += " • недоступно на этом GL-драйвере";
            final int selected = value;
            addAction(page, label,
                    value == 1800
                            ? "1800p доступно только при достаточном лимите текстур/буферов GPU"
                            : "Высота внутреннего кадра " + value + " пикселей",
                    () -> {
                        if (!supportsResolution(selected)) {
                            toast("Этот режим выше проверенного лимита графического драйвера.");
                            return;
                        }
                        homePrefs().edit().putInt("resolution", selected).apply();
                        buildSettingsPage();
                    }, false, !supported);
        }
        String aaText = selectedResolution < 960 ? "Сглаживание: базовое"
                : selectedResolution < 1440 ? "Сглаживание: supersampling x2"
                : selectedResolution == 1440 ? "Сглаживание: supersampling x3"
                : "Сглаживание: supersampling 3.75x";
        addInfoCard(page, aaText,
                "Flycast использует повышенное внутреннее разрешение как supersampling. Отдельный MSAA-переключатель не показывается, потому что в этом Android-бэкенде он не предоставлен.");

        TextView filterHeading = sectionHeading("РЕТРО-ФИЛЬТРЫ");
        page.addView(filterHeading, fullWidthWrap());
        addSubtitle(page, "Фильтр применяется к финальному кадру OpenGL. Можно выбрать один эффект или выключить.");
        String[] filterNames = {"ВЫКЛЮЧЕН", "CRT", "СТАРЫЙ ТЕЛЕВИЗОР", "COMPOSITE", "RGB MASK", "VHS"};
        String[] filterDescriptions = {
                "Оригинальная картинка без дополнительного эффекта",
                "Эффект ЭЛТ: сканирующие линии и лёгкое затемнение краёв",
                "Мягкая аналоговая картинка с оттенком и линиями",
                "Композитный видеосигнал с лёгким цветовым смешением",
                "RGB-сетка субпикселей",
                "VHS: цветовое смещение и аналоговые полосы"
        };
        int filter = homePrefs().getInt("filter", 0);
        for (int i = 0; i < filterNames.length; i++) {
            final int selected = i;
            addAction(page, (filter == i ? "✓ " : "") + filterNames[i],
                    filterDescriptions[i],
                    () -> {
                        homePrefs().edit().putInt("filter", selected).apply();
                        buildSettingsPage();
                    }, false);
        }

        addInfoCard(page, "АВТОРЕЖИМ PAL / NTSC",
                "Регион и частота не фиксируются. Выставлен режим Flycast Default, чтобы игра использовала свой регион и родную частоту 50/60 Гц. Искусственное ограничение до 30 FPS не включается.");
        addInfoCard(page, "КАДРОВАЯ СИНХРОНИЗАЦИЯ",
                "VSync и штатный Frame Pacing Flycast включены. Пропуск кадров не форсируется. Скорость эмуляции не замедляется ради совпадения с частотой экрана.");

        addAction(page, "СОХРАНИТЬ НАСТРОЙКИ",
                "Записать выбранный профиль, разрешение и фильтр в emu.cfg",
                () -> {
                    try {
                        saveFlycastSettings();
                        toast("Настройки сохранены. Они применятся при следующем запуске игры.");
                    } catch (Exception e) {
                        toast("Не удалось сохранить настройки: " + e.getMessage());
                    }
                }, true);
        addAction(page, "← НА ГЛАВНЫЙ ЭКРАН", "Вернуться в меню", this::buildHome, false);
        scroll.addView(page);
        setContentView(scroll);
        focusFirst(page);
    }

    private void chooseProfile(String profile) {
        homePrefs().edit()
                .putString("profile", profile)
                .putInt("resolution", profileResolution(profile))
                .apply();
        buildSettingsPage();
    }

    private int profileResolution(String profile) {
        if ("performance".equals(profile)) return 480;
        if ("quality".equals(profile)) return 1440;
        return 720;
    }

    private void saveFlycastSettings() throws Exception {
        int resolution = homePrefs().getInt("resolution",
                profileResolution(homePrefs().getString("profile", DEFAULT_PROFILE)));
        if (!supportsResolution(resolution))
            throw new IllegalStateException("выбранное разрешение выше лимита GPU");

        File home = getFlycastHome();
        if (!home.exists() && !home.mkdirs())
            throw new IllegalStateException("не удалось создать папку Flycast");
        File config = new File(home, "emu.cfg");
        List<String> lines = new ArrayList<>();
        if (config.isFile()) {
            BufferedReader reader = new BufferedReader(new InputStreamReader(
                    new FileInputStream(config), StandardCharsets.UTF_8));
            try {
                String line;
                while ((line = reader.readLine()) != null) lines.add(line);
            } finally {
                reader.close();
            }
        }

        String profile = homePrefs().getString("profile", DEFAULT_PROFILE);
        int anisotropic = "quality".equals(profile) ? 4 : ("balanced".equals(profile) ? 2 : 1);
        int filter = homePrefs().getInt("filter", 0);
        putIniValue(lines, "config", "rend.Resolution", String.valueOf(resolution));
        putIniValue(lines, "config", "rend.VideoFilter", String.valueOf(filter));
        putIniValue(lines, "config", "rend.ThreadedRendering", "yes");
        putIniValue(lines, "config", "rend.vsync", "yes");
        putIniValue(lines, "config", "rend.FramePacing", "yes");
        putIniValue(lines, "config", "pvr.AutoSkipFrame", "0");
        putIniValue(lines, "config", "ta.skip", "0");
        putIniValue(lines, "config", "rend.AnisotropicFiltering", String.valueOf(anisotropic));
        putIniValue(lines, "config", "rend.LinearInterpolation", "yes");
        putIniValue(lines, "config", "rend.EmulateFramebuffer", "no");
        putIniValue(lines, "config", "Dreamcast.Region", "3");
        putIniValue(lines, "config", "Dreamcast.Broadcast", "4");

        FileOutputStream out = new FileOutputStream(config, false);
        try {
            for (String line : lines) {
                out.write(line.getBytes(StandardCharsets.UTF_8));
                out.write('\n');
            }
            out.flush();
            out.getFD().sync();
        } finally {
            out.close();
        }
    }

    private void putIniValue(List<String> lines, String section, String key, String value) {
        String header = "[" + section + "]";
        int sectionStart = -1;
        int sectionEnd = lines.size();
        for (int i = 0; i < lines.size(); i++) {
            String trimmed = lines.get(i).trim();
            if (trimmed.equalsIgnoreCase(header)) {
                sectionStart = i;
                break;
            }
        }
        if (sectionStart < 0) {
            if (!lines.isEmpty() && !lines.get(lines.size() - 1).trim().isEmpty())
                lines.add("");
            lines.add(header);
            sectionStart = lines.size() - 1;
            sectionEnd = lines.size();
        } else {
            for (int i = sectionStart + 1; i < lines.size(); i++) {
                String trimmed = lines.get(i).trim();
                if (trimmed.startsWith("[") && trimmed.endsWith("]")) {
                    sectionEnd = i;
                    break;
                }
            }
        }

        for (int i = sectionStart + 1; i < sectionEnd; i++) {
            String line = lines.get(i);
            int equal = line.indexOf('=');
            if (equal > 0 && line.substring(0, equal).trim().equalsIgnoreCase(key)) {
                lines.set(i, key + " = " + value);
                return;
            }
        }
        lines.add(sectionEnd, key + " = " + value);
    }

    private File getFlycastHome() {
        SharedPreferences defaults = PreferenceManager.getDefaultSharedPreferences(this);
        String configured = defaults.getString("home_directory", "");
        if (configured != null && !configured.trim().isEmpty()) return new File(configured);
        File dir = getExternalFilesDir(null);
        return dir != null ? dir : getFilesDir();
    }

    private void pickGame() {
        Intent picker = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        picker.addCategory(Intent.CATEGORY_OPENABLE);
        picker.setType("*/*");
        picker.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        picker.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        startActivityForResult(picker, REQUEST_PICK_GAME);
    }

    private void pickGameFolder() {
        Intent picker = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        picker.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
                | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
        startActivityForResult(picker, REQUEST_PICK_FOLDER);
    }

    private void pickCover(String gameUri) {
        coverTargetUri = gameUri;
        Intent picker = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        picker.addCategory(Intent.CATEGORY_OPENABLE);
        picker.setType("image/*");
        picker.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        startActivityForResult(picker, REQUEST_PICK_COVER);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null) return;
        try {
            int flags = data.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION;
            if (flags != 0 && data.getData() != null)
                getContentResolver().takePersistableUriPermission(data.getData(), flags);
        } catch (SecurityException ignored) { }

        if (requestCode == REQUEST_PICK_GAME) {
            if (data.getClipData() != null) {
                int readFlags = data.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION;
                for (int i = 0; i < data.getClipData().getItemCount(); i++) {
                    Uri uri = data.getClipData().getItemAt(i).getUri();
                    try {
                        if (readFlags != 0)
                            getContentResolver().takePersistableUriPermission(uri, readFlags);
                    } catch (SecurityException ignored) { }
                    rememberGame(uri, queryDisplayName(uri));
                }
                buildLibraryPage();
            } else if (data.getData() != null) {
                Uri uri = data.getData();
                rememberGame(uri, queryDisplayName(uri));
                launchCore(uri);
            }
        } else if (requestCode == REQUEST_PICK_FOLDER && data.getData() != null) {
            Uri treeUri = data.getData();
            try {
                int flags = data.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION
                        | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                getContentResolver().takePersistableUriPermission(treeUri, flags);
            } catch (SecurityException ignored) { }
            scanFolder(treeUri);
        } else if (requestCode == REQUEST_PICK_COVER && data.getData() != null
                && coverTargetUri != null) {
            homePrefs().edit().putString("cover_" + coverTargetUri, data.getData().toString()).apply();
            coverTargetUri = null;
            buildLibraryPage();
        }
    }

    private void scanFolder(Uri treeUri) {
        toast("Сканирую папку с играми…");
        Thread scanner = new Thread(() -> {
            int added = 0;
            try {
                DocumentFile root = DocumentFile.fromTreeUri(this, treeUri);
                if (root != null && root.isDirectory()) {
                    List<GameItem> found = new ArrayList<>();
                    scanDirectory(root, 0, found);
                    added = rememberGames(found);
                }
            } catch (Throwable ignored) {
                // A removable-storage provider may disappear or revoke access during scanning.
            }
            final int totalAdded = added;
            runOnUiThread(() -> {
                toast(totalAdded > 0
                        ? "Добавлено игр: " + totalAdded
                        : "Совместимые игры не найдены или папка недоступна.");
                buildLibraryPage();
            });
        }, "DreamcastLibraryScan");
        scanner.setDaemon(true);
        scanner.start();
    }

    private void scanDirectory(DocumentFile directory, int depth, List<GameItem> found) {
        if (depth > MAX_SCAN_DEPTH || found.size() >= MAX_SCAN_FILES) return;
        DocumentFile[] children;
        try {
            children = directory.listFiles();
        } catch (Throwable ignored) {
            return;
        }
        for (DocumentFile child : children) {
            if (found.size() >= MAX_SCAN_FILES) return;
            String name = child.getName();
            if (name == null || name.startsWith(".")) continue;
            if (child.isDirectory()) {
                scanDirectory(child, depth + 1, found);
            } else if (child.isFile() && isGameFile(name)) {
                found.add(new GameItem(child.getUri().toString(), name));
            }
        }
    }

    private boolean isGameFile(String name) {
        String lower = name.toLowerCase(Locale.ROOT);
        return lower.endsWith(".gdi") || lower.endsWith(".chd")
                || lower.endsWith(".cdi") || lower.endsWith(".cue")
                || lower.endsWith(".zip") || lower.endsWith(".7z")
                || lower.endsWith(".lst");
    }

    private boolean rememberGame(Uri uri, String name) {
        return uri != null && rememberGame(uri.toString(), name);
    }

    private boolean rememberGame(String uri, String name) {
        if (uri == null || uri.trim().isEmpty()) return false;
        List<GameItem> incoming = new ArrayList<>();
        incoming.add(new GameItem(uri, (name == null || name.trim().isEmpty())
                ? "Игра Dreamcast" : name));
        return rememberGames(incoming) > 0;
    }

    private int rememberGames(List<GameItem> incoming) {
        List<GameItem> previous = readLibrary();
        List<GameItem> next = new ArrayList<>();
        java.util.HashSet<String> seen = new java.util.HashSet<>();
        int added = 0;

        // New scan results go first; remove duplicates and preserve existing order after them.
        for (GameItem item : incoming) {
            if (item == null || item.uri == null || item.uri.trim().isEmpty()) continue;
            if (seen.add(item.uri)) {
                if (previous.stream().noneMatch(old -> old.uri.equals(item.uri))) added++;
                if (next.size() < MAX_LIBRARY) next.add(item);
            }
        }
        for (GameItem item : previous) {
            if (next.size() >= MAX_LIBRARY) break;
            if (seen.add(item.uri)) next.add(item);
        }

        SharedPreferences.Editor editor = homePrefs().edit();
        for (int i = 0; i < MAX_LIBRARY; i++) {
            if (i < next.size()) {
                editor.putString("game_uri_" + i, next.get(i).uri);
                editor.putString("game_name_" + i, next.get(i).name);
            } else {
                editor.remove("game_uri_" + i);
                editor.remove("game_name_" + i);
            }
        }
        editor.putInt("game_count", next.size()).apply();
        return added;
    }

    private List<GameItem> readLibrary() {
        SharedPreferences prefs = homePrefs();
        int count = Math.min(MAX_LIBRARY, prefs.getInt("game_count", 0));
        List<GameItem> games = new ArrayList<>();
        for (int i = 0; i < count; i++) {
            String uri = prefs.getString("game_uri_" + i, null);
            if (uri != null) games.add(new GameItem(uri,
                    prefs.getString("game_name_" + i, "Игра Dreamcast")));
        }
        return games;
    }

    private void addGameCard(LinearLayout parent, GameItem game) {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.HORIZONTAL);
        card.setGravity(Gravity.CENTER_VERTICAL);
        card.setPadding(dp(14), dp(10), dp(14), dp(10));
        card.setBackground(actionBackground(false));
        ImageView cover = new ImageView(this);
        cover.setScaleType(ImageView.ScaleType.CENTER_CROP);
        cover.setBackground(rounded(Color.rgb(37, 48, 67), 0));
        cover.setImageResource(android.R.drawable.ic_menu_gallery);
        String coverUri = homePrefs().getString("cover_" + game.uri, null);
        if (coverUri != null) {
            try { cover.setImageURI(Uri.parse(coverUri)); } catch (Throwable ignored) { }
        }
        LinearLayout.LayoutParams coverParams = new LinearLayout.LayoutParams(dp(86), dp(86));
        coverParams.rightMargin = dp(16);
        card.addView(cover, coverParams);

        LinearLayout details = new LinearLayout(this);
        details.setOrientation(LinearLayout.VERTICAL);
        details.setGravity(Gravity.CENTER_VERTICAL);
        TextView name = new TextView(this);
        name.setText(game.name);
        name.setTextColor(foreground);
        name.setTextSize(19);
        name.setTypeface(Typeface.DEFAULT_BOLD);
        name.setMaxLines(2);
        details.addView(name, fullWidthWrap());
        TextView hint = new TextView(this);
        hint.setText("Flycast • запуск");
        hint.setTextColor(muted);
        hint.setTextSize(13);
        details.addView(hint, fullWidthWrap());
        card.addView(details, new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        card.setFocusable(true);
        card.setClickable(true);
        card.setOnClickListener(v -> launchCore(Uri.parse(game.uri)));
        LinearLayout.LayoutParams rowParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        rowParams.bottomMargin = dp(9);
        parent.addView(card, rowParams);

        addAction(parent, "ОБЛОЖКА: " + game.name,
                coverUri == null ? "Добавить свою обложку из памяти или USB" : "Изменить выбранную обложку",
                () -> pickCover(game.uri), false);
    }

    private void launchCore(Uri uri) {
        if (uri == null) return;
        try {
            Intent core = new Intent(this, NativeGLActivity.class);
            core.setAction(Intent.ACTION_VIEW);
            core.setDataAndType(uri, "*/*");
            core.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            startActivity(core);
        } catch (Exception e) {
            toast("Не удалось открыть игру: " + e.getMessage());
        }
    }

    private void openCoreBrowser() {
        startActivity(new Intent(this, NativeGLActivity.class));
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

    private void addEyebrow(LinearLayout parent, String text) {
        TextView view = new TextView(this);
        view.setText(text);
        view.setTextColor(accent);
        view.setTextSize(13);
        view.setLetterSpacing(0.12f);
        view.setTypeface(Typeface.DEFAULT_BOLD);
        parent.addView(view, fullWidthWrap());
    }

    private void addTitle(LinearLayout parent, String text, int size) {
        TextView view = new TextView(this);
        view.setText(text);
        view.setTextColor(foreground);
        view.setTextSize(size);
        view.setTypeface(Typeface.DEFAULT_BOLD);
        view.setPadding(0, dp(7), 0, 0);
        parent.addView(view, fullWidthWrap());
    }

    private void addSubtitle(LinearLayout parent, String text) {
        TextView view = new TextView(this);
        view.setText(text);
        view.setTextColor(muted);
        view.setTextSize(15);
        view.setPadding(0, dp(3), 0, dp(17));
        parent.addView(view, fullWidthWrap());
    }

    private TextView sectionHeading(String text) {
        TextView view = new TextView(this);
        view.setText(text);
        view.setTextColor(accent);
        view.setTextSize(13);
        view.setTypeface(Typeface.DEFAULT_BOLD);
        view.setLetterSpacing(0.1f);
        view.setPadding(0, dp(20), 0, dp(8));
        return view;
    }

    private void addInfoCard(LinearLayout parent, String title, String description) {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(18), dp(13), dp(18), dp(13));
        card.setBackground(rounded(Color.rgb(17, 25, 39), Color.rgb(43, 57, 78)));
        TextView head = new TextView(this);
        head.setText(title);
        head.setTextColor(accent);
        head.setTypeface(Typeface.DEFAULT_BOLD);
        head.setTextSize(15);
        card.addView(head, fullWidthWrap());
        TextView body = new TextView(this);
        body.setText(description);
        body.setTextColor(muted);
        body.setTextSize(13);
        body.setPadding(0, dp(5), 0, 0);
        card.addView(body, fullWidthWrap());
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        p.bottomMargin = dp(10);
        parent.addView(card, p);
    }

    private void addAction(LinearLayout parent, String title, String description,
                           Runnable action, boolean primary) {
        addAction(parent, title, description, action, primary, false);
    }

    private void addAction(LinearLayout parent, String title, String description,
                           Runnable action, boolean primary, boolean disabled) {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setGravity(Gravity.CENTER_VERTICAL);
        card.setPadding(dp(20), dp(13), dp(20), dp(13));
        card.setFocusable(true);
        card.setClickable(true);
        card.setEnabled(!disabled);
        card.setAlpha(disabled ? 0.4f : 1f);
        card.setBackground(actionBackground(primary));
        if (!disabled) card.setOnClickListener(v -> action.run());

        TextView heading = new TextView(this);
        heading.setText(title);
        heading.setTextColor(primary ? Color.rgb(18, 22, 30) : foreground);
        heading.setTextSize(18);
        heading.setTypeface(Typeface.DEFAULT_BOLD);
        card.addView(heading, fullWidthWrap());
        TextView detail = new TextView(this);
        detail.setText(description);
        detail.setTextColor(primary ? Color.rgb(55, 53, 48) : muted);
        detail.setTextSize(13);
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

    private void addFooter(LinearLayout page) {
        TextView footer = new TextView(this);
        footer.setText("Игры и BIOS не входят в приложение. Используйте собственные файлы. Сохранения и VMU обслуживает оригинальный движок Flycast.");
        footer.setTextColor(muted);
        footer.setTextSize(12);
        footer.setGravity(Gravity.CENTER);
        footer.setPadding(0, dp(24), 0, dp(6));
        page.addView(footer, fullWidthWrap());
    }

    private void focusFirst(LinearLayout page) {
        page.setFocusableInTouchMode(true);
        page.requestFocus();
        if (page.getChildCount() > 0) {
            View first = page.getChildAt(1);
            if (first != null) first.requestFocus();
        }
    }

    private LinearLayout.LayoutParams fullWidthWrap() {
        return new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    private int dp(float value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private void toast(String message) {
        Toast.makeText(this, message, Toast.LENGTH_LONG).show();
    }

    private static final class GameItem {
        final String uri;
        final String name;
        GameItem(String uri, String name) {
            this.uri = uri;
            this.name = name;
        }
    }
}
