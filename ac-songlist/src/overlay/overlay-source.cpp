#include "overlay-source.hpp"
#include "../plugin.hpp"
#include "../plugin-support.hpp"

#include <obs-module.h>

namespace acsl {

namespace {

struct Defaults {
	int width, height, fontSize;
};
// tamano inicial comodo para cada modo
const Defaults kDefaults[] = {{900, 180, 40}, {640, 460, 36}, {1500, 560, 36}};

struct OverlaySource {
	obs_source_t *source = nullptr;
	std::shared_ptr<SourceShared> shared = std::make_shared<SourceShared>();
	gs_texture_t *texture = nullptr;
	uint32_t texW = 0, texH = 0;
	std::atomic<uint32_t> width{900}, height{180};
};

QColor colorFromObs(long long value, int alpha = 255)
{
	// OBS guarda los colores como 0xAABBGGRR
	return QColor(int(value & 0xff), int((value >> 8) & 0xff), int((value >> 16) & 0xff), alpha);
}

#ifdef _WIN32
const char *kDefaultFont = "Segoe UI";
#elif defined(__APPLE__)
const char *kDefaultFont = "Helvetica Neue";
#else
const char *kDefaultFont = "Sans Serif";
#endif

void readSettings(OverlaySource *ctx, obs_data_t *settings)
{
	OverlayStyle s;
	s.mode = OverlayStyle::Mode(qBound(0, int(obs_data_get_int(settings, "mode")), 2));
	s.width = qBound(64, int(obs_data_get_int(settings, "width")), 4096);
	s.height = qBound(32, int(obs_data_get_int(settings, "height")), 4096);

	obs_data_t *font = obs_data_get_obj(settings, "font");
	if (font) {
		s.fontFamily = QString::fromUtf8(obs_data_get_string(font, "face"));
		const int size = int(obs_data_get_int(font, "size"));
		if (size > 0)
			s.fontSize = size;
		const long long flags = obs_data_get_int(font, "flags");
		s.bold = (flags & OBS_FONT_BOLD) != 0;
		s.italic = (flags & OBS_FONT_ITALIC) != 0;
		obs_data_release(font);
	}

	s.textColor = colorFromObs(obs_data_get_int(settings, "text_color"));
	s.mutedColor = s.textColor;
	s.mutedColor.setAlpha(168);
	s.accentColor = colorFromObs(obs_data_get_int(settings, "accent_color"));
	s.backgroundColor = colorFromObs(obs_data_get_int(settings, "bg_color"));
	s.backgroundOpacity = int(obs_data_get_int(settings, "bg_opacity"));
	s.radius = int(obs_data_get_int(settings, "radius"));
	s.padding = int(obs_data_get_int(settings, "padding"));
	s.align = OverlayStyle::Align(qBound(0, int(obs_data_get_int(settings, "align")), 2));
	s.valign = OverlayStyle::VAlign(qBound(0, int(obs_data_get_int(settings, "valign")), 2));
	s.accentBar = obs_data_get_bool(settings, "accent_bar");
	s.showLabel = obs_data_get_bool(settings, "show_label");
	s.label = QString::fromUtf8(obs_data_get_string(settings, "label"));
	s.showArtist = obs_data_get_bool(settings, "show_artist");
	s.showRequester = obs_data_get_bool(settings, "show_requester");
	s.showNumbers = obs_data_get_bool(settings, "show_numbers");
	s.hideWhenEmpty = obs_data_get_bool(settings, "hide_empty");
	s.idleText = QString::fromUtf8(obs_data_get_string(settings, "idle_text"));
	s.rows = qBound(1, int(obs_data_get_int(settings, "rows")), 40);
	s.columns = qBound(1, int(obs_data_get_int(settings, "columns")), 6);

	ctx->width = uint32_t(s.width);
	ctx->height = uint32_t(s.height);
	{
		std::lock_guard<std::mutex> lock(ctx->shared->mutex);
		ctx->shared->style = s;
		ctx->shared->listOnDemand = obs_data_get_bool(settings, "list_on_demand");
		ctx->shared->pageSeconds = qBound(2, int(obs_data_get_int(settings, "page_seconds")), 120);
		ctx->shared->styleVersion++;
	}
	Plugin::notifySourcesChanged();
}

const char *sourceName(void *)
{
	return obs_module_text("Source.Name");
}

void *sourceCreate(obs_data_t *settings, obs_source_t *source)
{
	auto *ctx = new OverlaySource;
	ctx->source = source;
	readSettings(ctx, settings);
	Plugin::addSource(ctx->shared);
	return ctx;
}

void sourceDestroy(void *data)
{
	auto *ctx = static_cast<OverlaySource *>(data);
	ctx->shared->alive = false;
	Plugin::notifySourcesChanged();
	if (ctx->texture) {
		obs_enter_graphics();
		gs_texture_destroy(ctx->texture);
		obs_leave_graphics();
	}
	delete ctx;
}

void sourceUpdate(void *data, obs_data_t *settings)
{
	readSettings(static_cast<OverlaySource *>(data), settings);
}

uint32_t sourceWidth(void *data)
{
	return static_cast<OverlaySource *>(data)->width;
}

uint32_t sourceHeight(void *data)
{
	return static_cast<OverlaySource *>(data)->height;
}

void sourceShow(void *data)
{
	static_cast<OverlaySource *>(data)->shared->showing = true;
	Plugin::notifySourcesChanged();
}

void sourceHide(void *data)
{
	static_cast<OverlaySource *>(data)->shared->showing = false;
}

// Hilo grafico: sube la imagen nueva (si la hay) y la dibuja.
void sourceRender(void *data, gs_effect_t *effect)
{
	auto *ctx = static_cast<OverlaySource *>(data);

	QImage image;
	{
		std::lock_guard<std::mutex> lock(ctx->shared->mutex);
		if (ctx->shared->imageDirty) {
			image = std::move(ctx->shared->image);
			ctx->shared->image = QImage();
			ctx->shared->imageDirty = false;
		}
	}
	if (!image.isNull()) {
		const uint32_t w = uint32_t(image.width()), h = uint32_t(image.height());
		if (ctx->texture && (w != ctx->texW || h != ctx->texH)) {
			gs_texture_destroy(ctx->texture);
			ctx->texture = nullptr;
		}
		if (!ctx->texture) {
			const uint8_t *bits = image.constBits();
			ctx->texture = gs_texture_create(w, h, GS_RGBA, 1, &bits, GS_DYNAMIC);
			ctx->texW = w;
			ctx->texH = h;
		} else {
			gs_texture_set_image(ctx->texture, image.constBits(), uint32_t(image.bytesPerLine()), false);
		}
	}
	if (!ctx->texture)
		return;

	// La imagen viene con alfa premultiplicado.
	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	gs_eparam_t *param = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture_srgb(param, ctx->texture);
	gs_draw_sprite(ctx->texture, 0, ctx->texW, ctx->texH);
	gs_blend_state_pop();
	gs_enable_framebuffer_srgb(previous);
}

void setVisible(obs_properties_t *props, const char *name, bool visible)
{
	if (obs_property_t *p = obs_properties_get(props, name))
		obs_property_set_visible(p, visible);
}

bool modeChanged(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	const int mode = qBound(0, int(obs_data_get_int(settings, "mode")), 2);
	const bool np = mode == OverlayStyle::NowPlaying;
	const bool queue = mode == OverlayStyle::Queue;
	const bool list = mode == OverlayStyle::SongList;
	setVisible(props, "accent_bar", np);
	setVisible(props, "idle_text", np);
	setVisible(props, "show_requester", np || queue);
	setVisible(props, "show_numbers", queue || list);
	setVisible(props, "hide_empty", queue);
	setVisible(props, "rows", queue || list);
	setVisible(props, "columns", list);
	setVisible(props, "list_on_demand", list);
	setVisible(props, "page_seconds", list);

	// Si el tamano sigue siendo el inicial de otro modo, usa el de este.
	const int w = int(obs_data_get_int(settings, "width")), h = int(obs_data_get_int(settings, "height"));
	for (int i = 0; i < 3; i++) {
		if (i != mode && w == kDefaults[i].width && h == kDefaults[i].height) {
			obs_data_set_int(settings, "width", kDefaults[mode].width);
			obs_data_set_int(settings, "height", kDefaults[mode].height);
			break;
		}
	}
	return true;
}

obs_properties_t *sourceProperties(void *)
{
	obs_properties_t *props = obs_properties_create();

	obs_property_t *mode = obs_properties_add_list(props, "mode", obs_module_text("Source.Mode"),
						       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(mode, obs_module_text("Source.Mode.NowPlaying"), OverlayStyle::NowPlaying);
	obs_property_list_add_int(mode, obs_module_text("Source.Mode.Queue"), OverlayStyle::Queue);
	obs_property_list_add_int(mode, obs_module_text("Source.Mode.SongList"), OverlayStyle::SongList);
	obs_property_set_modified_callback(mode, modeChanged);

	obs_properties_add_int(props, "width", obs_module_text("Source.Width"), 64, 4096, 10);
	obs_properties_add_int(props, "height", obs_module_text("Source.Height"), 32, 4096, 10);

	obs_properties_add_font(props, "font", obs_module_text("Source.Font"));
	obs_properties_add_color(props, "text_color", obs_module_text("Source.TextColor"));
	obs_properties_add_color(props, "accent_color", obs_module_text("Source.AccentColor"));
	obs_properties_add_color(props, "bg_color", obs_module_text("Source.BgColor"));
	obs_properties_add_int_slider(props, "bg_opacity", obs_module_text("Source.BgOpacity"), 0, 100, 1);
	obs_properties_add_int_slider(props, "radius", obs_module_text("Source.Radius"), 0, 60, 1);
	obs_properties_add_int_slider(props, "padding", obs_module_text("Source.Padding"), 0, 80, 1);

	obs_property_t *align = obs_properties_add_list(props, "align", obs_module_text("Source.Align"),
							OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(align, obs_module_text("Source.Align.Left"), OverlayStyle::Left);
	obs_property_list_add_int(align, obs_module_text("Source.Align.Center"), OverlayStyle::Center);
	obs_property_list_add_int(align, obs_module_text("Source.Align.Right"), OverlayStyle::Right);
	obs_property_t *valign = obs_properties_add_list(props, "valign", obs_module_text("Source.VAlign"),
							 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(valign, obs_module_text("Source.VAlign.Top"), OverlayStyle::Top);
	obs_property_list_add_int(valign, obs_module_text("Source.VAlign.Middle"), OverlayStyle::Middle);
	obs_property_list_add_int(valign, obs_module_text("Source.VAlign.Bottom"), OverlayStyle::Bottom);

	obs_properties_add_bool(props, "accent_bar", obs_module_text("Source.AccentBar"));
	obs_properties_add_bool(props, "show_label", obs_module_text("Source.ShowLabel"));
	obs_properties_add_text(props, "label", obs_module_text("Source.Label"), OBS_TEXT_DEFAULT);
	obs_properties_add_bool(props, "show_artist", obs_module_text("Source.ShowArtist"));
	obs_properties_add_bool(props, "show_requester", obs_module_text("Source.ShowRequester"));
	obs_properties_add_bool(props, "show_numbers", obs_module_text("Source.ShowNumbers"));
	obs_properties_add_text(props, "idle_text", obs_module_text("Source.IdleText"), OBS_TEXT_DEFAULT);
	obs_properties_add_bool(props, "hide_empty", obs_module_text("Source.HideEmpty"));
	obs_properties_add_int(props, "rows", obs_module_text("Source.Rows"), 1, 40, 1);
	obs_properties_add_int(props, "columns", obs_module_text("Source.Columns"), 1, 6, 1);
	obs_properties_add_bool(props, "list_on_demand", obs_module_text("Source.ListOnDemand"));
	obs_properties_add_int(props, "page_seconds", obs_module_text("Source.PageSeconds"), 2, 120, 1);
	return props;
}

void sourceDefaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "mode", OverlayStyle::NowPlaying);
	obs_data_set_default_int(settings, "width", kDefaults[0].width);
	obs_data_set_default_int(settings, "height", kDefaults[0].height);

	obs_data_t *font = obs_data_create();
	obs_data_set_default_string(font, "face", kDefaultFont);
	obs_data_set_default_int(font, "size", kDefaults[0].fontSize);
	obs_data_set_default_int(font, "flags", OBS_FONT_BOLD);
	obs_data_set_default_obj(settings, "font", font);
	obs_data_release(font);

	obs_data_set_default_int(settings, "text_color", 0xFFFFFFFF);
	obs_data_set_default_int(settings, "accent_color", 0xFFF8BD38); // #38BDF8
	obs_data_set_default_int(settings, "bg_color", 0xFF141010);     // #101014
	obs_data_set_default_int(settings, "bg_opacity", 85);
	obs_data_set_default_int(settings, "radius", 14);
	obs_data_set_default_int(settings, "padding", 22);
	obs_data_set_default_int(settings, "align", OverlayStyle::Left);
	obs_data_set_default_int(settings, "valign", OverlayStyle::Top);
	obs_data_set_default_bool(settings, "accent_bar", true);
	obs_data_set_default_bool(settings, "show_label", true);
	obs_data_set_default_string(settings, "label", "");
	obs_data_set_default_bool(settings, "show_artist", true);
	obs_data_set_default_bool(settings, "show_requester", true);
	obs_data_set_default_bool(settings, "show_numbers", true);
	obs_data_set_default_string(settings, "idle_text", "");
	obs_data_set_default_bool(settings, "hide_empty", false);
	obs_data_set_default_int(settings, "rows", 6);
	obs_data_set_default_int(settings, "columns", 3);
	obs_data_set_default_bool(settings, "list_on_demand", false);
	obs_data_set_default_int(settings, "page_seconds", 8);
}

} // namespace

void registerOverlaySource()
{
	obs_source_info info = {};
	info.id = "ac_songlist_source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB;
	info.icon_type = OBS_ICON_TYPE_TEXT;
	info.get_name = sourceName;
	info.create = sourceCreate;
	info.destroy = sourceDestroy;
	info.update = sourceUpdate;
	info.get_defaults = sourceDefaults;
	info.get_properties = sourceProperties;
	info.get_width = sourceWidth;
	info.get_height = sourceHeight;
	info.video_render = sourceRender;
	info.show = sourceShow;
	info.hide = sourceHide;
	obs_register_source(&info);
}

} // namespace acsl
