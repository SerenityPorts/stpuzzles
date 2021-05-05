#include <AK/Random.h>
#include <LibGfx/Path.h>
#include <LibGUI/Action.h>
#include <LibGUI/Application.h>
#include <LibGUI/Menu.h>
#include <LibGUI/Menubar.h>
#include <LibGUI/Painter.h>
#include <LibGUI/Widget.h>
#include <LibGUI/Window.h>
#include <stdio.h>
#include <unistd.h>
#include "puzzles.h"

void draw_text(void *handle, int x, int y, int fonttype, int fontsize,
            int align, int colour, const char *text);
void draw_rect(void *handle, int x, int y, int w, int h, int colour);
void draw_line(void *handle, int x1, int y1, int x2, int y2,
            int colour);
void draw_polygon(void *handle, int *coords, int npoints,
            int fillcolour, int outlinecolour);
void draw_circle(void *handle, int cx, int cy, int radius,
        int fillcolour, int outlinecolour);
void draw_update(void *handle, int x, int y, int w, int h);
void clip(void *handle, int x, int y, int w, int h);
void unclip(void *handle);
void start_draw(void *handle);
void end_draw(void *handle);
void status_bar(void *handle, const char *text);
blitter *blitter_new(void *handle, int w, int h);
void blitter_free(void *handle, blitter *bl);
void blitter_save(void *handle, blitter *bl, int x, int y);
void blitter_load(void *handle, blitter *bl, int x, int y);
void begin_doc(void *handle, int pages);
void begin_page(void *handle, int number);
void begin_puzzle(void *handle, float xm, float xc,
            float ym, float yc, int pw, int ph, float wmm);
void end_puzzle(void *handle);
void end_page(void *handle, int number);
void end_doc(void *handle);
void line_width(void *handle, float width);
void line_dotted(void *handle, bool dotted);
char *text_fallback(void *handle, const char *const *strings,
            int nstrings);
void draw_thick_line(void *handle, float thickness,
            float x1, float y1, float x2, float y2,
            int colour);

const drawing_api dr_api = {
    draw_text,
    draw_rect,
    draw_line,
    draw_polygon,
    draw_circle,
    draw_update,
    clip,
    unclip,
    start_draw,
    end_draw,
    status_bar,
    blitter_new,
    blitter_free,
    blitter_save,
    blitter_load,
    begin_doc,
    begin_page,
    begin_puzzle,
    end_puzzle,
    end_page,
    end_doc,
    line_width,
    line_dotted,
    text_fallback,
    draw_thick_line
};

class Frontend;

struct frontend {
    Frontend* frontend;
};

struct blitter {
    Gfx::Bitmap* bitmap;
    int x, y, w, h;
};

class Frontend : public GUI::Widget {
    C_OBJECT(Frontend);
public:
    void new_game() {
        midend_new_game(m_midend);
        m_width = 640, m_height = 480;
        midend_size(m_midend, &m_width, &m_height, true);
        m_window->resize(m_width, m_height);
        m_framebuffer = Gfx::Bitmap::create(Gfx::BitmapFormat::BGRx8888, {m_width, m_height});
        m_painter = make<GUI::Painter>(*m_framebuffer);
        int ncolors = 0;
        auto colors = midend_colours(m_midend, &ncolors);
        for (int i = 0; i < ncolors * 3; i += 3) {
            m_colors.append({colors[i] * 255, colors[i + 1] * 255, colors[i + 2] * 255});
        }
        int id_limit;
        m_preset_menu = midend_get_presets(m_midend, &id_limit);
        midend_redraw(m_midend);
    }

    void set_game_params(game_params* params) {
        midend_set_params(m_midend, params);
        new_game();
    }

    void restart_game() {
        midend_restart_game(m_midend);
    }

    void solve_game() {
        midend_solve(m_midend);
    }

    preset_menu* get_presets() {
        return m_preset_menu;
    }

    ~Frontend() {
        midend_free(m_midend);
    }

    void draw_rect(int x, int y, int w, int h, int colour) {
        m_painter->fill_rect({x, y, w, h}, get_color(colour));
    }

    void activate_timer() {
        if (!m_timer_enabled) {
            start_timer(20);
            m_timer_enabled = true;
        }
    }

    void deactivate_timer() {
        if (m_timer_enabled) {
            stop_timer();
            m_timer_enabled = false;
        }
    }

    void draw_update(int x, int y, int w, int h) {
        update();
    }

    void draw_text(int x, int y, int fonttype, int fontsize,
            int align, int colour, const char *text) {
        // FIXME: text alignment
        auto text_view = StringView(text);
        m_painter->draw_text({x, y, fontsize * text_view.length(), fontsize}, text_view, Gfx::TextAlignment::TopLeft, get_color(colour));
    }

    void draw_polygon(int *coords, int npoints,
            int fillcolour, int outlinecolour) {
        VERIFY(outlinecolour != -1);
        Gfx::Path polygon;
        polygon.move_to({coords[0], coords[1]});
        for (int i = 2; i < npoints * 2; i += 2) {
            polygon.line_to({coords[i], coords[i + 1]});
        }
        polygon.line_to({coords[0], coords[1]});
        m_painter->fill_path(polygon, get_color(fillcolour));
        m_painter->stroke_path(polygon, get_color(outlinecolour), 1);
    }

    void draw_circle(int cx, int cy, int radius,
        int fillcolour, int outlinecolour) {
        auto x = cx - radius, y = cy - radius, size = 2 * radius;
        auto rect_size = floor(size / M_SQRT2), x_offset = ceil(x + (radius - rect_size / 2)), y_offset = ceil(y + (radius - rect_size / 2));
        m_painter->fill_ellipse({x, y, size, size}, get_color(fillcolour));
        m_painter->draw_ellipse_intersecting({x_offset, y_offset, rect_size, rect_size}, get_color(outlinecolour));
    }

    void draw_line(float x1, float y1, float x2, float y2,
            int colour) {
        m_painter->draw_line({x1, y1}, {x2, y2}, get_color(colour));
    }

    void draw_thick_line(float thickness,
            float x1, float y1, float x2, float y2,
            int colour) {
        m_painter->draw_line({x1, y1}, {x2, y2}, get_color(colour), thickness);
    }

    void clip(int x, int y, int w, int h) {
        m_painter->add_clip_rect({x, y, w, h});
    }

    void unclip() {
        m_painter->clear_clip_rect();
    }

    blitter *blitter_new(int w, int h) {
        auto blitter = snew(struct blitter);
        blitter->w = w;
        blitter->h = h;
        blitter->bitmap = Gfx::Bitmap::create(Gfx::BitmapFormat::BGRx8888, {w, h}).leak_ref();
        return blitter;
    }

    void blitter_free(blitter *bl) {
        sfree(bl);
    }

    void blitter_save(blitter *bl, int x, int y) {
        bl->x = x;
        bl->y = y;
        GUI::Painter painter(*bl->bitmap);
        painter.blit({0, 0}, *m_framebuffer, {x, y, bl->w, bl->h});
    }

    void blitter_load(blitter *bl, int x, int y) {
        if (x == BLITTER_FROMSAVED) x = bl->x;
        if (y == BLITTER_FROMSAVED) y = bl->y;
        m_painter->blit({x, y}, *bl->bitmap, {0, 0, bl->w, bl->h});
    }

    int width() { return m_width; }
    int height() { return m_height; }
private:
    Frontend(NonnullRefPtr<GUI::Window> window) : m_window(window) {
        m_midend = midend_new(&m_frontend, m_game, &dr_api, this);
        new_game();
    }

    virtual void paint_event(GUI::PaintEvent& event) override {
        GUI::Painter painter(*this);
        painter.draw_scaled_bitmap(event.rect(), *m_framebuffer, m_framebuffer->rect());
    }

    virtual void timer_event(Core::TimerEvent& event) override {
        midend_timer(m_midend, 0.02);
        update();
    }

    virtual void mousedown_event(GUI::MouseEvent& event) override {
        int button = LEFT_BUTTON;
        if (event.button() == GUI::MouseButton::Middle) {
            button = MIDDLE_BUTTON;
        }
        else if (event.button() == GUI::MouseButton::Right) {
            button = RIGHT_BUTTON;
        }
        if (!midend_process_key(m_midend, event.position().x(), event.position().y(), button)) GUI::Application::the()->quit();
    }

    virtual void mouseup_event(GUI::MouseEvent& event) override {
        int button = LEFT_RELEASE;
        if (event.button() == GUI::MouseButton::Middle) {
            button = MIDDLE_RELEASE;
        }
        else if (event.button() == GUI::MouseButton::Right) {
            button = RIGHT_RELEASE;
        }
        if (!midend_process_key(m_midend, event.position().x(), event.position().y(), button)) GUI::Application::the()->quit();
    }

    virtual void mousemove_event(GUI::MouseEvent& event) override {
        int button = LEFT_DRAG;
        if (event.button() == GUI::MouseButton::Middle) {
            button = MIDDLE_DRAG;
        }
        else if (event.button() == GUI::MouseButton::Right) {
            button = RIGHT_DRAG;
        }
        if (!midend_process_key(m_midend, event.position().x(), event.position().y(), button)) GUI::Application::the()->quit();
    }

    virtual void keydown_event(GUI::KeyEvent& event) override {
        int button;
        switch (event.key()) {
        case Key_Up:
            button = CURSOR_UP;
            break;
        case Key_Down:
            button = CURSOR_DOWN;
            break;
        case Key_Left:
            button = CURSOR_LEFT;
            break;
        case Key_Right:
            button = CURSOR_RIGHT;
            break;
        default:
            button = event.code_point();
            break;
        }
        if (!midend_process_key(m_midend, 0, 0, button)) GUI::Application::the()->quit();  
    }

    Gfx::Color get_color(int n) {
        if (n == -1) return Gfx::Color::Transparent;
        return m_colors[n];
    }

    frontend m_frontend { this };
    midend* m_midend;
    const game* m_game { &thegame };
    preset_menu* m_preset_menu;
    int m_width { 640 }, m_height { 480 };
    Vector<Gfx::Color> m_colors {};
    RefPtr<Gfx::Bitmap> m_framebuffer;
    OwnPtr<GUI::Painter> m_painter;
    bool m_timer_enabled { false };
    NonnullRefPtr<GUI::Window> m_window;
};

void draw_text(void *handle, int x, int y, int fonttype, int fontsize,
            int align, int colour, const char *text) {
    ((Frontend*)handle)->draw_text(x, y, fonttype, fontsize, align, colour, text);
}

void draw_rect(void *handle, int x, int y, int w, int h, int colour) {
    ((Frontend*)handle)->draw_rect(x, y, w, h, colour);
}

void draw_line(void *handle, int x1, int y1, int x2, int y2,
            int colour) {
    ((Frontend*)handle)->draw_line(x1, y1, x2, y2, colour);
}

void draw_polygon(void *handle, int *coords, int npoints,
            int fillcolour, int outlinecolour) {
    ((Frontend*)handle)->draw_polygon(coords, npoints, fillcolour, outlinecolour);
}

void draw_circle(void *handle, int cx, int cy, int radius,
        int fillcolour, int outlinecolour) {
    ((Frontend*)handle)->draw_circle(cx, cy, radius, fillcolour, outlinecolour);
}

void draw_update(void *handle, int x, int y, int w, int h) {
    ((Frontend*)handle)->draw_update(x, y, w, h);
}

void clip(void *handle, int x, int y, int w, int h) {
    ((Frontend*)handle)->clip(x, y, w, h);
}

void unclip(void *handle) {
    ((Frontend*)handle)->unclip();
}

void start_draw(void *handle) {
    // TODO();
}

void end_draw(void *handle) {
    // TODO();
}

void status_bar(void *handle, const char *text) {
    // TODO();
}

blitter *blitter_new(void *handle, int w, int h) {
    return ((Frontend*)handle)->blitter_new(w, h);
}

void blitter_free(void *handle, blitter *bl) {
    ((Frontend*)handle)->blitter_free(bl);
}

void blitter_save(void *handle, blitter *bl, int x, int y) {
    ((Frontend*)handle)->blitter_save(bl, x, y);
}

void blitter_load(void *handle, blitter *bl, int x, int y) {
    ((Frontend*)handle)->blitter_load(bl, x, y);
}

void begin_doc(void *handle, int pages) {
    TODO();
}

void begin_page(void *handle, int number) {
    TODO();
}

void begin_puzzle(void *handle, float xm, float xc,
            float ym, float yc, int pw, int ph, float wmm) {
    TODO();
}

void end_puzzle(void *handle) {
    TODO();
}

void end_page(void *handle, int number) {
    TODO();
}

void end_doc(void *handle) {
    TODO();
}

void line_width(void *handle, float width) {
    TODO();
}

void line_dotted(void *handle, bool dotted) {
    TODO();
}

char *text_fallback(void *handle, const char *const *strings,
            int nstrings) {
    TODO();
}

void draw_thick_line(void *handle, float thickness,
            float x1, float y1, float x2, float y2,
            int colour) {
    ((Frontend*)handle)->draw_thick_line(thickness, x1, y1, x2, y2, colour);
}

void fatal(const char *fmt, ...) {
    va_list var_list;
    va_start(var_list, fmt);
    vprintf(fmt, var_list);
    va_end(var_list);
    exit(1);
}

void frontend_default_colour(frontend *fe, float *output) {
    output[0] = output[1] = output[2] = 0.75;
}

void deactivate_timer(frontend *fe) {
    fe->frontend->deactivate_timer();
}

void activate_timer(frontend *fe) {
    fe->frontend->activate_timer();
}

void get_random_seed(void **randseed, int *randseedsize) {
    auto seed = snew(int);
    *seed = get_random<int>();
    *randseed = seed;
    *randseedsize = sizeof(int);
}

struct param_ref {
    NonnullRefPtr<GUI::Action> action;
    game_params* params;
};

Vector<struct param_ref> g_params;

void create_preset_menu(GUI::Menu& menu, Frontend& frontend, preset_menu* presets, const int id = 0) {
    for (int i = 0; i < presets->n_entries; i++) {
        auto* preset = &presets->entries[i];
        if (preset->params) {
            auto action = GUI::Action::create(preset->title, [&](auto& action) {
                for (auto ref : g_params)
                    if (ref.action == &action) {
                        frontend.set_game_params(ref.params);
                        break;
                    }
            });
            g_params.append({action, preset->params});
            menu.add_action(action);
        }
        else {
            auto& submenu = menu.add_submenu(preset->title);
            create_preset_menu(submenu, frontend, preset->submenu);
        }
    }
}

int main(int argc, char** argv) {
    if (pledge("stdio rpath accept wpath cpath recvfd sendfd unix fattr", nullptr) < 0) {
        perror("pledge");
        return 1;
    }

    auto app = GUI::Application::construct(argc, argv);

    auto window = GUI::Window::construct();
    window->set_title(thegame.name);
    auto& frontend = window->set_main_widget<Frontend>(window);
    window->set_resizable(false);

    auto menubar = GUI::Menubar::construct();
    auto& game_menu = menubar->add_menu("&Game");
    game_menu.add_action(GUI::Action::create("&New Game", [&](auto&) {
        frontend.new_game();
    }));
    game_menu.add_action(GUI::Action::create("&Restart Game", [&](auto&) {
        frontend.restart_game();
    }));
    game_menu.add_action(GUI::Action::create("&Solve Game", [&](auto&) {
        frontend.solve_game();
    }));

    auto presets = frontend.get_presets();
    auto& presets_menu = menubar->add_menu("&Type");
    create_preset_menu(presets_menu, frontend, presets);

    window->set_menubar(move(menubar));
    window->show();

    return app->exec();
}
