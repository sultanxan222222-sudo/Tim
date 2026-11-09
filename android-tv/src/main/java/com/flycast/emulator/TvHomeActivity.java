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
import android.provider.OpenableColumns;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

/** TV-first launcher connected to the real Flycast Android activity. */
public final class TvHomeActivity extends Activity {
    private static final int REQUEST_PICK_GAME = 4101;
    private static final String PREFS = "dreamcast_tv_home";
    private static final int MAX_RECENTS = 5;

    private final int accent = Color.rgb(255, 190, 48);
    private final int foreground = Color.rgb(242, 245, 250);
    private final int muted = Color.rgb(160, 172, 190);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
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
                android.view.View.SYSTEM_UI_FLAG_FULLSCREEN
                        | android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | android.view.View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | android.view.View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
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
                "Открыть игру из памяти приставки или USB-накопителя", this::pickGame, true);

        List<RecentGame> recents = readRecents();
        if (!recents.isEmpty()) {
            TextView recentHeading = new TextView(this);
            recentHeading.setText("НЕДАВНИЕ ИГРЫ");
            recentHeading.setTextColor(accent);
            recentHeading.setTextSize(13);
            recentHeading.setTypeface(Typeface.DEFAULT_BOLD);
            recentHeading.setLetterSpacing(0.1f);
            recentHeading.setPadding(0, dp(22), 0, dp(8));
            page.addView(recentHeading, fullWidthWrap());

            for (RecentGame game : recents) {
                addAction(page, game.name, "Запустить игру",
                        () -> launchCore(Uri.parse(game.uri)), false);
            }
        }

        TextView coreHeading = new TextView(this);
        coreHeading.setText("ИНТЕРФЕЙС ЭМУЛЯТОРА");
        coreHeading.setTextColor(accent);
        coreHeading.setTextSize(13);
        coreHeading.setTypeface(Typeface.DEFAULT_BOLD);
        coreHeading.setLetterSpacing(0.1f);
        coreHeading.setPadding(0, dp(22), 0, dp(8));
        page.addView(coreHeading, fullWidthWrap());

        addAction(page, "Открыть Flycast",
                "Встроенная библиотека, настройки ядра и управление сохранениями",
                this::openCoreBrowser, false);

        TextView footer = new TextView(this);
        footer.setText("Игры и BIOS не входят в приложение. Используйте собственные файлы.");
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
