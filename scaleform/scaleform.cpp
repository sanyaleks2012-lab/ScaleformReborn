#include <fstream>
#include <filesystem>
#include <string>

#ifdef WIN32
#include <minmax.h>
#else
#include <algorithm>
#endif

#include "scaleform.hpp"
#include "../init.hpp"
#include "../sdk.hpp"
#include "../config.hpp"

#include "sanitary_macros.hpp"

JAVASCRIPT base =
#include "base.js"
;

JAVASCRIPT alerts =
#include "alerts.js"
;

JAVASCRIPT teamcount_avatar =
#include "teamcount_avatar.js"
;

// ${buyZone} - percentage
#define BUYZONE "${buyZone}"
JAVASCRIPT buyzone =
#include "buyzone.js"
;

// ${isShort} - is healthammo style short
#define HEALTHAMMO_STYLE "${isShort}"
JAVASCRIPT healthammo =
#include "healthammo.js"
;

JAVASCRIPT spectating =
#include "spectating.js"
;

JAVASCRIPT weapon_select =
#include "weapon_select.js"
;

// ${isCt} - whether winner team is CT
// ${isT} - whether winner team is T
// ${pendingMvp} - there's a mvp for the round
// ${is2013} - is 2013 winpanel or post 2013
#define IS_CT "${isCt}"
#define IS_T "${isT}"
#define PENDING_MVP "${pendingMvp}"
#define IS_2013 "${is2013}"
JAVASCRIPT winpanel =
#include "winpanel.js"
;

// ${radarColor} - radar hex
// ${dashboardLabelColor} - dashboard label hex
#define RADAR_COLOR "${radarColor}"
#define DASHBOARD_LABEL_COLOR "${dashboardLabelColor}"
JAVASCRIPT color =
#include "color.js"
;

// ${alpha} - alpha float
#define ALPHA "${alpha}"
JAVASCRIPT alpha =
#include "alpha.js"
;
#define MAX_ALPHA 1.F

// Old colors
// Note: for dashboard label we just append 'B2' (70% opac)
// for radar elements we just add '19' (25% opac)
HEX_COLOR colors[11] =
{
    "#F5FFC0", // Classic
    "#FFFFFF", // White
    "#C4ECFE", // Light Blue
    "#6E9CFB", // Dark Blue
    "#F294FF", // Purple
    "#FF6057", // Red
    "#FFA557", // Orange
    "#FEFD55", // Yellow
    "#78FF51", // Green
    "#57FFBD", // Pale Green/Aqua
    "#FFADC4"  // Pink
};

#define kRadarColor 0
#define kDashColor 1
static std::string get_color(int n, int color_type)
{
    if (color_type == kRadarColor)
        return std::string(colors[n]) + "19";
    else if (color_type == kDashColor)
        return std::string(colors[n]) + "B2";
    else return std::string(colors[n]);
};

// utilities

static void replace_str(std::string& s, const std::string& search, const std::string& replace) {
    for (size_t pos = 0;;pos += replace.length()) {
        // Locate the substring to replace
        pos = s.find(search, pos);
        if (pos == std::string::npos)
            break;
        // Replace by erasing and inserting
        s.erase(pos, search.length());
        s.insert(pos, replace);
    }
}

static tsf::ui_panel_t* get_panel(const char* id)
{
    tsf::ui_engine_t* engine = ctx.i.panorama->access_ui_engine();
    if (!engine)
        return nullptr;

    tsf::ui_panel_t* parent = engine->get_last_dispatched_event_target_panel();
    if (!parent)
        return nullptr;

    tsf::ui_panel_t* itr = parent;
    while (itr && engine->is_valid_panel_pointer(itr)) {
        if (!strcmp(itr->get_id(), id))
            return itr;

        itr = itr->get_parent();
    }

    return nullptr;
}

// internal

void scaleform_init()
{
    scf.root = scf.weap_sel = scf.weap_pan_bg = nullptr;
    scf.inited = false;
    scf.old_color = scf.old_healthammo_style = -1;
    scf.old_alpha = -1.f;
    scf.old_in_buyzone = -1;
    scf.pending_mvp = false;
    scf.old_weap_rows_count = 0;
}

static void scaleform_teamcount_avatar()
{
    tsf::ui_engine_t* engine = ctx.i.panorama->access_ui_engine();
    if (!engine)
        return LOG("Failed Scaleform Team Count event (ui engine)\n");

    DEBUG("Teamcount being edited!\n");
    engine->run_script(scf.root, teamcount_avatar, CSGO_HUD_SCHEMA);
}

static void scaleform_weapon_selection()
{
    tsf::ui_engine_t* engine = ctx.i.panorama->access_ui_engine();
    if (!engine)
        return LOG("Failed Scaleform Weapon Selection event (ui engine)\n");

    DEBUG("Weapon selection being edited!\n");
    engine->run_script(scf.root, weapon_select, CSGO_HUD_SCHEMA);
}

static void scaleform_spec()
{
    tsf::ui_engine_t* engine = ctx.i.panorama->access_ui_engine();
    if (!engine)
        return LOG("Failed Scaleform Spec event (ui engine)\n");

    DEBUG("Spec being edited!\n");
    engine->run_script(scf.root, spectating, CSGO_HUD_SCHEMA);
}

static void scaleform_alerts()
{
    tsf::ui_engine_t* engine = ctx.i.panorama->access_ui_engine();
    if (!engine)
        return LOG("Failed Scaleform Alerts event (ui engine)\n");

    DEBUG("Alerts being reapplied!\n");
    engine->run_script(scf.root, alerts, CSGO_HUD_SCHEMA);
}

// deathnotices has been removed; this function is kept as a stub for compatibility
static void scaleform_death()
{
    // no operation – deathnotices functionality is removed
    return;
}

void scaleform_install()
{
    if (!ctx.g.scf_on || scf.inited)
        return;

    if (scf.root = get_panel("CSGOHud"); !scf.root) {
        // cannot gracefully recover
        LOG("Failed Scaleform install (root panel)\n");
        return;
    }

    DEBUG("scf.root = %p\n", scf.root);

    if (tsf::ui_panel_t* parent = scf.root->find_child_traverse("HudWeaponPanel"); parent)
    {
        if (scf.weap_pan_bg = parent->find_child_traverse("WeaponPanelBottomBG"); !scf.weap_pan_bg)
        {
            return LOG("Failed Scaleform install (weap panel bg)\n");
        }
    }
    else return LOG("Failed Scaleform install (weap panel)\n");


    if (scf.weap_sel = scf.root->find_child_traverse("HudWeaponSelection"); !scf.weap_sel)
    {
        return LOG("Failed Scaleform install (weap sel)\n");
    }

    DEBUG("scf.weap_pan_bg = %p\n", scf.weap_pan_bg);
    DEBUG("scf.weap_sel = %p\n", scf.weap_sel);

    tsf::ui_engine_t* engine = ctx.i.panorama->access_ui_engine();
    if (!engine)
        return LOG("Failed Scaleform install (ui engine)\n");

    // install base modifications
    engine->run_script(scf.root, base, CSGO_HUD_SCHEMA);

    // alerts
    engine->run_script(scf.root, alerts, CSGO_HUD_SCHEMA);

    // anticipate events
    scaleform_teamcount_avatar();
    scaleform_weapon_selection();
    scaleform_spec();

    scf.inited = true;
    LOG("Scaleform installed!\n");
}

void scaleform_tick(tsf::player_t* local)
{
#ifdef __linux__
    static const Uint8* state = SDL_GetKeyboardState(NULL);
#endif

    // listen to user commands
    if (GET_ASYNC_KEY_STATE(SCALEFORM_TOGGLE_KEY) & 1)
    {
        ctx.g.scf_on = !ctx.g.scf_on;
        LOG("Toggled Scaleform %s\n", (ctx.g.scf_on ? "on" : "off"));
    }
    else if (GET_ASYNC_KEY_STATE(SCALEFORM_WINPANEL_TOGGLE_KEY) & 1)
    {
        ctx.g.old_wp = !ctx.g.old_wp;
        LOG("Toggled Scaleform winpanel to %s\n", (ctx.g.old_wp ? "old" : "new"));
    }
    else if (GET_ASYNC_KEY_STATE(SCALEFORM_WEAPON_SELECTION_RARITY_TOGGLE_KEY) & 1)
    {
        ctx.g.show_rarity = !ctx.g.show_rarity;
        LOG("Toggled Scaleform Weapon Selection Rarity to %s\n", (ctx.g.show_rarity ? "on" : "off"));
    }

    tsf::ui_engine_t* engine = ctx.i.panorama->access_ui_engine();
    if (!engine)
        return;

    // could also be potential recovery from (very unlikely to impossible)
    // fail in levelinit
    if (!scf.inited && ctx.g.scf_on)
    {
        scaleform_install();
        return;
    }
    else if (!scf.inited || !ctx.g.scf_on)
        return;

    if (GET_ASYNC_KEY_STATE(SCALEFORM_JAVASCRIPT_LOADER_KEY) & 1)
    {
        std::filesystem::path js_path = std::filesystem::current_path() / "base.js";
        if (std::filesystem::exists(js_path))
        {
            std::ifstream read(js_path);
            std::string js((std::istreambuf_iterator<char>(read)), std::istreambuf_iterator<char>());
            engine->run_script(scf.root, js.c_str(), CSGO_HUD_SCHEMA);
            LOG("Loaded JavaScript!\n");
        }
        else
        {
            LOG("Cannot find %s!\n", js_path.string().c_str());
        }
    }

    // TODO: other way around, fully reinitialzie schema on toggle off

    // tick events

    if (scf.weap_pan_bg) {
        // make always visible (needs to be done in C++ for performance)
        scf.weap_pan_bg->set_visible(true);
    }

    UPDATING_VAR(scf.old_color, n, WIN32_LINUX_LITERAL(min, std::min)((int)(std::size(colors) - 1), ctx.c.cl_hud_color->get_int()),
        {
            DEBUG("Changed hud color!\n");
            std::string js = std::string(color);
            replace_str(js, RADAR_COLOR, get_color(n, kRadarColor));
            replace_str(js, DASHBOARD_LABEL_COLOR, get_color(n, kDashColor));
            engine->run_script(scf.root, js.c_str(), CSGO_HUD_SCHEMA);
        });

    UPDATING_VAR(scf.old_alpha, n, WIN32_LINUX_LITERAL(min, std::min)(MAX_ALPHA, ctx.c.cl_hud_background_alpha->get_float()),
        {
            DEBUG("Changed hud alpha!\n");
            std::string js = std::string(alpha);
            char buf[16];
            sprintf(buf, "%.2f", n);
            replace_str(js, ALPHA, buf);
            engine->run_script(scf.root, js.c_str(), CSGO_HUD_SCHEMA);
        });

    UPDATING_VAR(scf.old_healthammo_style, n, ctx.c.cl_hud_healthammo_style->get_int(),
        {
            DEBUG("Changed hud healthammo style!\n");
            std::string js = std::string(healthammo);
            replace_str(js, HEALTHAMMO_STYLE, (n == 0 ? "true" : "false"));
            engine->run_script(scf.root, js.c_str(), CSGO_HUD_SCHEMA);
        });

    UPDATING_VAR(scf.old_in_buyzone, n, local->in_buyzone(),
        {
            DEBUG("Changed buyzone state!\n");
            std::string js = std::string(buyzone);
            replace_str(js, BUYZONE, n == 1 ? "100%" : "0%");
            engine->run_script(scf.root, js.c_str(), CSGO_HUD_SCHEMA);
        });

    // sorry for the very bad code
    // NOTE(para): it's so bad that i felt the need to clarify
    // for some reason valve really doesn't like this being stored on the
    // stack.
    // see here too 55 8B EC 83 3D ? ? ? ? ? 8B 15 ? ? ? ? 56 8B F1 C7 05
    constexpr size_t size = sizeof(tsf::utl_vector_t<tsf::ui_panel_t*>);
    auto vec = (tsf::utl_vector_t<tsf::ui_panel_t*> *)malloc(size);
    memset(vec, 0, size);
    scf.weap_sel->find_children_with_class_traverse("weapon-row", vec);
    int count = vec->size;
    free(vec);

    UPDATING_VAR(scf.old_weap_rows_count, n, count,
        {
            scaleform_weapon_selection();
        });
}

static void scaleform_winpanel(int team)
{
    tsf::ui_engine_t* engine = ctx.i.panorama->access_ui_engine();
    if (!engine)
        return LOG("Failed Scaleform Win Panel event (ui engine)\n");

    DEBUG("Winpanel being edited!\n");
    std::string js = std::string(winpanel);
    replace_str(js, IS_CT, (team == 3) ? "true" : "false");
    replace_str(js, IS_T, (team == 2) ? "true" : "false");
    replace_str(js, PENDING_MVP, scf.pending_mvp ? "true" : "false");
    replace_str(js, IS_2013, ctx.g.old_wp ? "true" : "false");
    engine->run_script(scf.root, js.c_str(), CSGO_HUD_SCHEMA);

    scf.pending_mvp = false;
}

void scaleform_on_event(tsf::event_t* event)
{
    if (!scf.inited)
        return;

    if (!strcmp(event->get_name(), "round_mvp"))
        scf.pending_mvp = true; // flag mvp
    // NOTE(para): relying on item_equip for certainty isn't great but it's
    // not going to be the downfall of anything.
    else if (!strcmp(event->get_name(), "round_start") || !strcmp(event->get_name(), "item_equip"))
        scaleform_weapon_selection();
    else if (!strcmp(event->get_name(), "round_end"))
        scaleform_winpanel(event->get_int("winner"));
}

void scaleform_after_event(const char* name)
{
    if (!scf.inited)
        return;

    if (!strcmp(name, "bot_takeover") || !strcmp(name, "switch_team") ||
        !strcmp(name, "round_start"))
    {
        scaleform_teamcount_avatar();
        scaleform_weapon_selection();
        scaleform_alerts();   // reapply alerts styles each round
    }
    else if (!strcmp(name, "spec_target_updated") || !strcmp(name, "spec_mode_updated") || !strcmp(name, "item_equip"))
        scaleform_spec();
    else if (!strcmp(name, "player_death"))
        scaleform_death();  // stub, no actual death notices
}

// Stub for icon replacement (data.hpp is no longer used)
bool scaleform_get_replacement_icon(const char* name, const uint8_t*& data, size_t& len, int& w, int& h)
{
    (void)name;
    (void)data;
    (void)len;
    (void)w;
    (void)h;
    return false;
}