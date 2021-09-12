#include "selfdrive/ui/paint.h"

#include <algorithm>
#include <cassert>

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#define NANOVG_GL3_IMPLEMENTATION
#define nvgCreate nvgCreateGL3
#else
#include <GLES3/gl3.h>
#define NANOVG_GLES3_IMPLEMENTATION
#define nvgCreate nvgCreateGLES3
#endif

#define NANOVG_GLES3_IMPLEMENTATION
#include <nanovg_gl.h>
#include <nanovg_gl_utils.h>

#include "selfdrive/common/timing.h"
#include "selfdrive/common/util.h"
#include "selfdrive/hardware/hw.h"

#include "selfdrive/ui/ui.h"
#include <iostream>
#include <time.h> // opkr
#include <string> // opkr
#include "selfdrive/ui/dashcam.h"
#include <cmath> //Neokii TPMS

static void ui_print(UIState *s, int x, int y,  const char* fmt, ... )
{
  char* msg_buf = NULL;
  va_list args;
  va_start(args, fmt);
  vasprintf( &msg_buf, fmt, args);
  va_end(args);
  nvgText(s->vg, x, y, msg_buf, NULL);
}

static void ui_draw_text(const UIState *s, float x, float y, const char *string, float size, NVGcolor color, const char *font_name) {
  nvgFontFace(s->vg, font_name);
  nvgFontSize(s->vg, size*0.8);
  nvgFillColor(s->vg, color);
  nvgText(s->vg, x, y, string, NULL);
}

// static void draw_chevron(UIState *s, float x, float y, float sz, NVGcolor fillColor, NVGcolor glowColor) {
//   // glow
//   float g_xo = sz/5;
//   float g_yo = sz/10;
//   nvgBeginPath(s->vg);
//   nvgMoveTo(s->vg, x+(sz*1.35)+g_xo, y+sz+g_yo);
//   nvgLineTo(s->vg, x, y-g_xo);
//   nvgLineTo(s->vg, x-(sz*1.35)-g_xo, y+sz+g_yo);
//   nvgClosePath(s->vg);
//   nvgFillColor(s->vg, glowColor);
//   nvgFill(s->vg);

//   // chevron
//   nvgBeginPath(s->vg);
//   nvgMoveTo(s->vg, x+(sz*1.25), y+sz);
//   nvgLineTo(s->vg, x, y);
//   nvgLineTo(s->vg, x-(sz*1.25), y+sz);
//   nvgClosePath(s->vg);
//   nvgFillColor(s->vg, fillColor);
//   nvgFill(s->vg);
// }

//atom(conan)'s steering wheel
static void ui_draw_circle_image_rotation(const UIState *s, int center_x, int center_y, int radius, const char *image, NVGcolor color, float img_alpha, float angleSteers = 0) {
  const int img_size = radius * 1.5;
  float img_rotation =  angleSteers/180*3.141592;
  int ct_pos = -radius * 0.75;

  nvgBeginPath(s->vg);
  nvgCircle(s->vg, center_x, center_y + (bdr_s+7), radius);
  nvgFillColor(s->vg, color);
  nvgFill(s->vg);
  //ui_draw_image(s, {center_x - (img_size / 2), center_y - (img_size / 2), img_size, img_size}, image, img_alpha);

  nvgSave( s->vg );
  nvgTranslate(s->vg, center_x, (center_y + (bdr_s*1.5)));
  nvgRotate(s->vg, -img_rotation);  

  ui_draw_image(s, {ct_pos, ct_pos, img_size, img_size}, image, img_alpha);
  nvgRestore(s->vg); 
}

static void ui_draw_circle_image(const UIState *s, int center_x, int center_y, int radius, const char *image, bool active) {
  float bg_alpha = active ? 0.3f : 0.1f;
  float img_alpha = active ? 1.0f : 0.15f;
  if (s->scene.monitoring_mode) {
    ui_draw_circle_image_rotation(s, center_x, center_y, radius, image, nvgRGBA(10, 120, 20, (255 * bg_alpha * 1.1)), img_alpha);
  } else {
    ui_draw_circle_image_rotation(s, center_x, center_y, radius, image, nvgRGBA(0, 0, 0, (255 * bg_alpha)), img_alpha);
  }
}

static void draw_lead(UIState *s, const cereal::ModelDataV2::LeadDataV3::Reader &lead_data, const vertex_data &vd) {
  // Draw lead car indicator
  auto [x, y] = vd;

  float fillAlpha = 0;
  float speedBuff = 10.;
  float leadBuff = 40.;
  float d_rel = lead_data.getX()[0];
  float v_rel = lead_data.getV()[0];
  if (d_rel < leadBuff) {
    fillAlpha = 255*(1.0-(d_rel/leadBuff));
    if (v_rel < 0) {
      fillAlpha += 255*(-1*(v_rel/speedBuff));
    }
    fillAlpha = (int)(fmin(fillAlpha, 255));
  }

  float sz = std::clamp((30 * 30) / (d_rel / 2 + 15), 12.0f, 60.0f) * 2.35;
  //float sz = std::clamp((25 * 30) / (d_rel / 3 + 30), 15.0f, 30.0f) * 2.35;
  x = std::clamp(x, 0.f, s->fb_w - sz / 2);
  y = std::fmin(s->fb_h - sz * .6, y);
  nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

  int sz_w = sz * 2;
  int sz_h = sz * 1;
  int x_l = x - sz_w;
  int y_l = y;

  if (s->scene.radarDistance < 149) {                                         //radar가 인식되면
    //draw_chevron(s, x, y, sz, nvgRGBA(201, 34, 49, fillAlpha), COLOR_ORANGE); //orange ==> red
    //ui_draw_text(s, x, y + sz/1.5f, "R", 20 * 2.5, COLOR_WHITE, "sans-bold");
    ui_draw_image(s, {x_l, y_l, sz_w * 2, sz_h}, "lead_under_radar", 0.8f);  
  } else {                                                                                 //camera가 인식되면
    //draw_chevron(s, x, y, sz, nvgRGBA(150, 0, 200, fillAlpha), nvgRGBA(0, 150, 200, 200)); //oceanblue ==> purple
    //ui_draw_text(s, x, y + sz/1.5f, "C", 20 * 2.5, COLOR_ORANGE, "sans-bold");
    ui_draw_image(s, {x_l, y_l, sz_w * 2, sz_h}, "lead_under_camera", 0.8f);  
  }
}

static float lock_on_rotation[] = {0.f, 0.1f*NVG_PI, 0.3f*NVG_PI, 0.6f*NVG_PI, 1.0f*NVG_PI, 1.4f*NVG_PI, 1.7f*NVG_PI, 1.9f*NVG_PI, 2.0f*NVG_PI};

static float lock_on_scale[] = {1.f, 1.05f, 1.1f, 1.15f, 1.2f, 1.15f, 1.1f, 1.05f, 1.f, 0.95f, 0.9f, 0.85f, 0.8f, 0.85f, 0.9f, 0.95f};

static void draw_lead_radar(UIState *s, const cereal::ModelDataV2::LeadDataV3::Reader &lead_data, const vertex_data &vd) {
    auto [x, y] = vd;
    float d_rel = lead_data.getX()[0];
    auto intrinsic_matrix = s->wide_camera ? ecam_intrinsic_matrix : fcam_intrinsic_matrix;
    float zoom = ZOOM / intrinsic_matrix.v[0];
    float sz = std::clamp((30 * 30) / (d_rel / 2 + 15), 12.0f, 60.0f) * zoom;
    // float sz = std::clamp((25 * 30) / (d_rel / 3 + 30), 15.0f, 30.0f) * zoom;
    x = std::clamp(x, 0.f, s->fb_w - sz / 2);
    if(d_rel < 30) {
      const float c = 0.7f;
      float r = d_rel * ((1.f - c) / 30.f) + c;
      if(r > 0.f)
        y = y * r;
    }
    y = std::fmin(s->fb_h - sz * .6, y);
    y = std::fmin(s->fb_h * 0.8f, y);
    float img_alpha = 0.8f;
    const char* image = "lead_radar";
    if(s->sm->frame % 2 == 0) {
        s->lock_on_anim_index++;
    }
    int img_size = 60;
    if(d_rel < 100) {
        img_size = (int)(float(-3./5.) * d_rel + 120);
    }
    nvgSave(s->vg);
    nvgTranslate(s->vg, x, y - (img_size / 2) );
    nvgRotate(s->vg, lock_on_rotation[s->lock_on_anim_index % 9]);
    ui_draw_image(s, {-(img_size / 2), -(img_size / 2), img_size, img_size}, image, img_alpha);
    nvgRestore(s->vg);
}

static void draw_lead_custom(UIState *s, const cereal::ModelDataV2::LeadDataV3::Reader &lead_data, const vertex_data &vd) {
    auto [x, y] = vd;
    float d_rel = lead_data.getX()[0];
    auto intrinsic_matrix = s->wide_camera ? ecam_intrinsic_matrix : fcam_intrinsic_matrix;
    float zoom = ZOOM / intrinsic_matrix.v[0];
    float sz = std::clamp((25 * 30) / (d_rel / 3 + 30), 15.0f, 30.0f) * zoom;
    x = std::clamp(x, 0.f, s->fb_w - sz / 2);
    if(d_rel < 30) {
      const float c = 0.7f;
      float r = d_rel * ((1.f - c) / 30.f) + c;
      if(r > 0.f)
        y = y * r;
    }
    y = std::fmin(s->fb_h - sz * .6, y);
    y = std::fmin(s->fb_h * 0.8f, y);
    float img_alpha = 0.8f;
    const char* image = "custom_lead_radar";
    if(s->sm->frame % 2 == 0) {
        s->lock_on_anim_index++;
    }
    int img_size = 80;
    if(d_rel < 100) {
        img_size = (int)(float(-2./5.) * d_rel + 120);
    }
    nvgSave(s->vg);
    nvgTranslate(s->vg, x, y);
    nvgRotate(s->vg, lock_on_rotation[s->lock_on_anim_index % 9]);
    ui_draw_image(s, {-(img_size / 2), -(img_size / 2), img_size, img_size}, image, img_alpha);
    nvgRestore(s->vg);
}

static void draw_side_lead_custom(UIState *s, const cereal::ModelDataV2::LeadDataV3::Reader &lead_data, const vertex_data &vd) {
    auto [x, y] = vd;
    float d_rel = lead_data.getX()[0];
    float sz = std::clamp((25 * 30) / (d_rel / 3 + 30), 15.0f, 30.0f) * 2.35;
    x = std::clamp(x, 0.f, s->fb_w - sz / 2);
    y = std::fmin(s->fb_h - sz * .6, y);

    float img_alpha = 0.8f;
    const char* image = "custom_lead_vision";
    if(s->sm->frame % 2 == 0) {
        s->lock_on_anim_index++;
    }

    int img_size = 80;
    if(d_rel < 100) {
        img_size = (int)(float(-2./5.) * d_rel + 120);
    }

    nvgSave(s->vg);
    nvgTranslate(s->vg, x, y);    
    float scale = lock_on_scale[s->lock_on_anim_index % 8];
    nvgScale(s->vg, scale, scale);
    ui_draw_image(s, {-(img_size / 2), -(img_size / 2), img_size, img_size}, image, img_alpha);
    nvgRestore(s->vg);
}

static void ui_draw_line(UIState *s, const line_vertices_data &vd, NVGcolor *color, NVGpaint *paint) {
  if (vd.cnt == 0) return;

  const vertex_data *v = &vd.v[0];
  nvgBeginPath(s->vg);
  nvgMoveTo(s->vg, v[0].x, v[0].y);
  for (int i = 1; i < vd.cnt; i++) {
    nvgLineTo(s->vg, v[i].x, v[i].y);
  }
  nvgClosePath(s->vg);
  if (color) {
    nvgFillColor(s->vg, *color);
  } else if (paint) {
    nvgFillPaint(s->vg, *paint);
  }
  nvgFill(s->vg);
}

static void draw_vision_frame(UIState *s) {
  glBindVertexArray(s->frame_vao);
  mat4 *out_mat = &s->rear_frame_mat;
  glActiveTexture(GL_TEXTURE0);

  if (s->last_frame) {
    glBindTexture(GL_TEXTURE_2D, s->texture[s->last_frame->idx]->frame_tex);
    if (!Hardware::EON()) {
      // this is handled in ion on QCOM
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, s->last_frame->width, s->last_frame->height,
                   0, GL_RGB, GL_UNSIGNED_BYTE, s->last_frame->addr);
    }
  }

  glUseProgram(s->gl_shader->prog);
  glUniform1i(s->gl_shader->getUniformLocation("uTexture"), 0);
  glUniformMatrix4fv(s->gl_shader->getUniformLocation("uTransform"), 1, GL_TRUE, out_mat->v);

  assert(glGetError() == GL_NO_ERROR);
  glEnableVertexAttribArray(0);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const void *)0);
  glDisableVertexAttribArray(0);
  glBindVertexArray(0);
}

static void ui_draw_vision_lane_lines(UIState *s) {
  const UIScene &scene = s->scene;
  NVGpaint track_bg;
  int steerOverride = scene.car_state.getSteeringPressed();
  float steer_max_v = scene.steerMax_V - (1.5 * (scene.steerMax_V - 0.9));
  int torque_scale = (int)fabs(255*(float)scene.output_scale*steer_max_v);
  int red_lvl = fmin(255, torque_scale);
  int green_lvl = fmin(255, 255-torque_scale);

  float red_lvl_line = 0;
  float green_lvl_line = 0;
  //if (!scene.end_to_end) {
  if (!scene.lateralPlan.lanelessModeStatus) {
    // paint lanelines, Hoya's colored lane line
    for (int i = 0; i < std::size(scene.lane_line_vertices); i++) {
      if (scene.lane_line_probs[i] > 0.4){
        red_lvl_line = 1.0 - ((scene.lane_line_probs[i] - 0.4) * 2.5);
        green_lvl_line = 1.0;
      } else {
        red_lvl_line = 1.0;
        green_lvl_line = 1.0 - ((0.4 - scene.lane_line_probs[i]) * 2.5);
      }
      NVGcolor color = nvgRGBAf(1.0, 1.0, 1.0, scene.lane_line_probs[i]);
      if (!scene.comma_stock_ui) {
        color = nvgRGBAf(red_lvl_line, green_lvl_line, 0, 1);
      }
      ui_draw_line(s, scene.lane_line_vertices[i], &color, nullptr);
    }

    // paint road edges
    for (int i = 0; i < std::size(scene.road_edge_vertices); i++) {
      NVGcolor color = nvgRGBAf(1.0, 0.0, 0.0, std::clamp<float>(1.0 - scene.road_edge_stds[i], 0.0, 1.0));
      ui_draw_line(s, scene.road_edge_vertices[i], &color, nullptr);
    }
  }
  if (scene.controls_state.getEnabled() && !scene.comma_stock_ui) {
    if (steerOverride) {
      track_bg = nvgLinearGradient(s->vg, s->fb_w, s->fb_h, s->fb_w, s->fb_h*.4,
        COLOR_BLACK_ALPHA(80), COLOR_BLACK_ALPHA(20));
    } else if (!scene.lateralPlan.lanelessModeStatus) {
      track_bg = nvgLinearGradient(s->vg, s->fb_w, s->fb_h, s->fb_w, s->fb_h*.4,
        nvgRGBA(red_lvl, green_lvl, 0, 150), nvgRGBA((int)(0.7*red_lvl), (int)(0.7*green_lvl), 0, 100));
    } else { // differentiate laneless mode color (Grace blue)
      track_bg = nvgLinearGradient(s->vg, s->fb_w, s->fb_h, s->fb_w, s->fb_h*.4,
        nvgRGBA(0, 100, 255, 250), nvgRGBA(0, 100, 255, 100));
    }
  } else {
    // Draw white vision track
    track_bg = nvgLinearGradient(s->vg, s->fb_w, s->fb_h, s->fb_w, s->fb_h * .4,
                                        COLOR_WHITE_ALPHA(150), COLOR_WHITE_ALPHA(20));
  }
  // paint path
  ui_draw_line(s, scene.track_vertices, nullptr, &track_bg);
}

// Draw all world space objects.
static void ui_draw_world(UIState *s) {
  const UIScene &scene = s->scene;
  nvgScissor(s->vg, 0, 0, s->fb_w, s->fb_h);

  // Draw lane edges and vision/mpc tracks
  ui_draw_vision_lane_lines(s);

  // Draw lead indicators if openpilot is handling longitudinal
  //if (s->scene.longitudinal_control) {

  auto lead_one = (*s->sm)["modelV2"].getModelV2().getLeadsV3()[0];
  auto lead_two = (*s->sm)["modelV2"].getModelV2().getLeadsV3()[1];  

  if (scene.lead_custom) {
    if (lead_one.getProb() > .5) {
      draw_lead_custom(s, lead_one, scene.lead_vertices[0]);
    }
    if (lead_two.getProb() > .5 && (std::abs(lead_one.getX()[0] - lead_two.getX()[0]) > 3.0)) {
      draw_side_lead_custom(s, lead_two, scene.lead_vertices[1]);    
    }
  }
  else {
    if (lead_one.getProb() > .5) {
      draw_lead_radar(s, lead_one, scene.lead_vertices[0]);
      draw_lead(s, lead_one, scene.lead_vertices[0]);
    }
    if (lead_two.getProb() > .5 && (std::abs(lead_one.getX()[0] - lead_two.getX()[0]) > 3.0)) {
      draw_lead(s, lead_two, scene.lead_vertices[1]);
    }
  }
  nvgResetScissor(s->vg);
}

// TPMS code added from Neokii
static NVGcolor get_tpms_color(float tpms) {
    if(tpms < 5 || tpms > 60) // N/A
        return nvgRGBA(255, 255, 255, 200);
    if(tpms < 30)
        return nvgRGBA(255, 90, 90, 200);
    return nvgRGBA(255, 255, 255, 200);
}

static std::string get_tpms_text(float tpms) {
    if(tpms < 5 || tpms > 60)
        return "";

    char str[32];
    snprintf(str, sizeof(str), "%.0f", round(tpms));
    return std::string(str);
}

static void ui_draw_tpms(UIState *s)
{
    auto car_state = (*s->sm)["carState"].getCarState();
    auto tpms = car_state.getTpms();

    const float fl = tpms.getFl();
    const float fr = tpms.getFr();
    const float rl = tpms.getRl();
    const float rr = tpms.getRr();

    const int w = 58;
    const int h = 126;
    int x = 1920 - 150 ;
    int y = 735 ;

    const int margin = 10;

    ui_draw_image(s, {x, y, w, h}, "tire_pressure", 0.8f);

    nvgFontSize(s->vg, 50);
    nvgFontFace(s->vg, "sans-bold");

    nvgTextAlign(s->vg, NVG_ALIGN_RIGHT);
    nvgFillColor(s->vg, get_tpms_color(fl));
    nvgText(s->vg, x-margin, y+50, get_tpms_text(fl).c_str(), NULL);

    nvgTextAlign(s->vg, NVG_ALIGN_LEFT);
    nvgFillColor(s->vg, get_tpms_color(fr));
    nvgText(s->vg, x+w+margin, y+50, get_tpms_text(fr).c_str(), NULL);

    nvgTextAlign(s->vg, NVG_ALIGN_RIGHT);
    nvgFillColor(s->vg, get_tpms_color(rl));
    nvgText(s->vg, x-margin, y+h-10, get_tpms_text(rl).c_str(), NULL);

    nvgTextAlign(s->vg, NVG_ALIGN_LEFT);
    nvgFillColor(s->vg, get_tpms_color(rr));
    nvgText(s->vg, x+w+margin, y+h-10, get_tpms_text(rr).c_str(), NULL);
}


// TPMS code added from OPKR
// static void ui_draw_tpms(UIState *s) {
//   const UIScene &scene = s->scene;
//   char tpmsFl[64];
//   char tpmsFr[64];
//   char tpmsRl[64];
//   char tpmsRr[64];
//   int viz_tpms_w = 230;
//   int viz_tpms_h = 160;
//   int viz_tpms_x = s->fb_w - (bdr_s+425);
//   int viz_tpms_y = bdr_s;
//   float maxv = 0;
//   float minv = 300;
//   const Rect rect = {viz_tpms_x, viz_tpms_y, viz_tpms_w, viz_tpms_h};

//   if (maxv < scene.tpmsPressureFl) {maxv = scene.tpmsPressureFl;}
//   if (maxv < scene.tpmsPressureFr) {maxv = scene.tpmsPressureFr;}
//   if (maxv < scene.tpmsPressureRl) {maxv = scene.tpmsPressureRl;}
//   if (maxv < scene.tpmsPressureRr) {maxv = scene.tpmsPressureRr;}
//   if (minv > scene.tpmsPressureFl) {minv = scene.tpmsPressureFl;}
//   if (minv > scene.tpmsPressureFr) {minv = scene.tpmsPressureFr;}
//   if (minv > scene.tpmsPressureRl) {minv = scene.tpmsPressureRl;}
//   if (minv > scene.tpmsPressureRr) {minv = scene.tpmsPressureRr;}

//   // Draw Border
//   ui_draw_rect(s->vg, rect, COLOR_WHITE_ALPHA(100), 10, 20.);
//   // Draw Background
//   if ((maxv - minv) > 3) {
//     ui_fill_rect(s->vg, rect, COLOR_RED_ALPHA(80), 20);
//   } else {
//     ui_fill_rect(s->vg, rect, COLOR_BLACK_ALPHA(80), 20);
//   }

//   nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
//   const int pos_x = viz_tpms_x + (viz_tpms_w / 2);
//   const int pos_y = viz_tpms_y + 45;
//   ui_draw_text(s, pos_x, pos_y, "TPMS(psi)", 45, COLOR_WHITE_ALPHA(180), "sans-regular");
//   snprintf(tpmsFl, sizeof(tpmsFl), "%.1f", scene.tpmsPressureFl);
//   snprintf(tpmsFr, sizeof(tpmsFr), "%.1f", scene.tpmsPressureFr);
//   snprintf(tpmsRl, sizeof(tpmsRl), "%.1f", scene.tpmsPressureRl);
//   snprintf(tpmsRr, sizeof(tpmsRr), "%.1f", scene.tpmsPressureRr);
//   if (scene.tpmsPressureFl < 34) {
//     ui_draw_text(s, pos_x-55, pos_y+50, tpmsFl, 60, COLOR_RED, "sans-bold");
//   } else if (scene.tpmsPressureFl > 50) {
//     ui_draw_text(s, pos_x-55, pos_y+50, "N/A", 60, COLOR_WHITE_ALPHA(200), "sans-semibold");
//   } else {
//     ui_draw_text(s, pos_x-55, pos_y+50, tpmsFl, 60, COLOR_WHITE_ALPHA(200), "sans-semibold");
//   }
//   if (scene.tpmsPressureFr < 34) {
//     ui_draw_text(s, pos_x+55, pos_y+50, tpmsFr, 60, COLOR_RED, "sans-bold");
//   } else if (scene.tpmsPressureFr > 50) {
//     ui_draw_text(s, pos_x+55, pos_y+50, "N/A", 60, COLOR_WHITE_ALPHA(200), "sans-semibold");
//   } else {
//     ui_draw_text(s, pos_x+55, pos_y+50, tpmsFr, 60, COLOR_WHITE_ALPHA(200), "sans-semibold");
//   }
//   if (scene.tpmsPressureRl < 34) {
//     ui_draw_text(s, pos_x-55, pos_y+100, tpmsRl, 60, COLOR_RED, "sans-bold");
//   } else if (scene.tpmsPressureRl > 50) {
//     ui_draw_text(s, pos_x-55, pos_y+100, "N/A", 60, COLOR_WHITE_ALPHA(200), "sans-semibold");
//   } else {
//     ui_draw_text(s, pos_x-55, pos_y+100, tpmsRl, 60, COLOR_WHITE_ALPHA(200), "sans-semibold");
//   }
//   if (scene.tpmsPressureRr < 34) {
//     ui_draw_text(s, pos_x+55, pos_y+100, tpmsRr, 60, COLOR_RED, "sans-bold");
//   } else if (scene.tpmsPressureRr > 50) {
//     ui_draw_text(s, pos_x+55, pos_y+100, "N/A", 60, COLOR_WHITE_ALPHA(200), "sans-semibold");
//   } else {
//     ui_draw_text(s, pos_x+55, pos_y+100, tpmsRr, 60, COLOR_WHITE_ALPHA(200), "sans-semibold");
//   }
// }

static void ui_draw_standstill(UIState *s) {
  const UIScene &scene = s->scene;

  int viz_standstill_x = s->fb_w - 560;
  int viz_standstill_y = bdr_s + 160 + 250;
  
  int minute = 0;
  int second = 0;

  minute = int(scene.lateralPlan.standstillElapsedTime / 60);
  second = int(scene.lateralPlan.standstillElapsedTime) - (minute * 60);

  if (scene.standStill) {
    nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
    nvgFontSize(s->vg, 125);
    nvgFillColor(s->vg, COLOR_ORANGE_ALPHA(240));
    ui_print(s, viz_standstill_x, viz_standstill_y, "STOP");
    nvgFontSize(s->vg, 150);
    nvgFillColor(s->vg, COLOR_WHITE_ALPHA(240));
    ui_print(s, viz_standstill_x, viz_standstill_y+150, "%01d:%02d", minute, second);
  }
}

static void ui_draw_debug(UIState *s) {
  const UIScene &scene = s->scene;

  int ui_viz_rx = bdr_s + 190;
  int ui_viz_ry = bdr_s;
  int ui_viz_rx_center = s->fb_w/2;
  
  nvgTextAlign(s->vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_MIDDLE);

  if (scene.nDebugUi1) {
    ui_draw_text(s, 450, ui_viz_ry+360, scene.alertTextMsg1.c_str(), 40, COLOR_WHITE_ALPHA(100), "sans-semibold");
    ui_draw_text(s, 450, ui_viz_ry+400, scene.alertTextMsg2.c_str(), 40, COLOR_WHITE_ALPHA(100), "sans-semibold");
  }
  
  nvgFillColor(s->vg, COLOR_WHITE_ALPHA(100));
  if (scene.nDebugUi2) {
    //if (scene.gpsAccuracyUblox != 0.00) {
    //  nvgFontSize(s->vg, 34);
    //  ui_print(s, 28, 28, "LAT／LON: %.5f／%.5f", scene.latitudeUblox, scene.longitudeUblox);
    //}
    nvgFontSize(s->vg, 37);
    //ui_print(s, ui_viz_rx, ui_viz_ry, "Live Parameters");
    ui_print(s, ui_viz_rx, ui_viz_ry+240, "SR:%.2f", scene.liveParams.steerRatio);
    //ui_print(s, ui_viz_rx, ui_viz_ry+100, "AOfs:%.2f", scene.liveParams.angleOffset);
    ui_print(s, ui_viz_rx, ui_viz_ry+280, "AA:%.2f", scene.liveParams.angleOffsetAverage);
    ui_print(s, ui_viz_rx, ui_viz_ry+320, "SF:%.2f", scene.liveParams.stiffnessFactor);

    ui_print(s, ui_viz_rx, ui_viz_ry+360, "AD:%.2f", scene.steer_actuator_delay);
    ui_print(s, ui_viz_rx, ui_viz_ry+400, "SC:%.2f", scene.lateralPlan.steerRateCost);
    ui_print(s, ui_viz_rx, ui_viz_ry+440, "OS:%.2f", abs(scene.output_scale));
    ui_print(s, ui_viz_rx, ui_viz_ry+480, "%.2f | %.2f", scene.lateralPlan.lProb, scene.lateralPlan.rProb);
    //ui_print(s, ui_viz_rx, ui_viz_ry+800, "A:%.5f", scene.accel_sensor2);
    if (scene.map_is_running) {
      if (scene.liveMapData.opkrspeedsign) ui_print(s, ui_viz_rx, ui_viz_ry+520, "SS:%.0f", scene.liveMapData.opkrspeedsign);
      if (scene.liveMapData.opkrspeedlimit) ui_print(s, ui_viz_rx, ui_viz_ry+560, "SL:%.0f", scene.liveMapData.opkrspeedlimit);
      if (scene.liveMapData.opkrspeedlimitdist) ui_print(s, ui_viz_rx, ui_viz_ry+600, "DS:%.0f", scene.liveMapData.opkrspeedlimitdist);
      if (scene.liveMapData.opkrturninfo) ui_print(s, ui_viz_rx, ui_viz_ry+640, "TI:%.0f", scene.liveMapData.opkrturninfo);
      if (scene.liveMapData.opkrdisttoturn) ui_print(s, ui_viz_rx, ui_viz_ry+680, "DT:%.0f", scene.liveMapData.opkrdisttoturn);
    }
    nvgFontSize(s->vg, 37);
    nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    if (scene.lateralControlMethod == 0) {
      ui_print(s, ui_viz_rx_center, ui_viz_ry+305, "PID");
    } else if (scene.lateralControlMethod == 1) {
      ui_print(s, ui_viz_rx_center, ui_viz_ry+305, "INDI");
    } else if (scene.lateralControlMethod == 2) {
      ui_print(s, ui_viz_rx_center, ui_viz_ry+305, "LQR");
    }
  }
}

/*
  park @1;
  drive @2;
  neutral @3;
  reverse @4;
  sport @5;
  low @6;
  brake @7;
  eco @8;
*/
static void ui_draw_gear( UIState *s ) {
  const UIScene &scene = s->scene;  
  NVGcolor nColor = COLOR_WHITE;
  int x_pos = s->fb_w - (90 + bdr_s);
  int y_pos = bdr_s + 140;
  int ngetGearShifter = int(scene.getGearShifter);
  //int  x_pos = 1795;
  //int  y_pos = 155;
  char str_msg[512];

  nvgFontFace(s->vg, "sans-bold");
  nvgFontSize(s->vg, 160 );
  switch( ngetGearShifter )
  {
    case 1 : strcpy( str_msg, "P" ); nColor = nvgRGBA(200, 200, 255, 255); break;
    case 2 : strcpy( str_msg, "D" ); nColor = COLOR_GREEN; break;
    case 3 : strcpy( str_msg, "N" ); nColor = COLOR_WHITE; break;
    case 4 : strcpy( str_msg, "R" ); nColor = COLOR_RED; break;
    case 7 : strcpy( str_msg, "B" ); break;
    default: sprintf( str_msg, "%d", ngetGearShifter ); break;
  }

  nvgFillColor(s->vg, nColor);
  ui_print( s, x_pos, y_pos, str_msg );
}

static void ui_draw_vision_face(UIState *s) {
  const int radius = 85;
  const int center_x = radius + bdr_s;
  const int center_y = 1080 - 85 - 30;
  ui_draw_circle_image(s, center_x, center_y, radius, "driver_face", s->scene.dm_active);
}

static void ui_draw_vision_scc_gap(UIState *s) {
  auto car_state = (*s->sm)["carState"].getCarState();
  int gap = car_state.getCruiseGapSet();

  const int w = 180;
  const int h = 180;
  const int x = 15;
  const int y = 700;
  
  if(gap <= 0) {ui_draw_image(s, {x, y, w, h}, "lead_car_dist_0", 0.3f);}
  else if (gap == 1) {ui_draw_image(s, {x, y, w, h}, "lead_car_dist_1", 0.5f);}
  else if (gap == 2) {ui_draw_image(s, {x, y, w, h}, "lead_car_dist_2", 0.5f);}
  else if (gap == 3) {ui_draw_image(s, {x, y, w, h}, "lead_car_dist_3", 0.5f);}
  else if (gap == 4) {ui_draw_image(s, {x, y, w, h}, "lead_car_dist_4", 0.5f);}
  else {ui_draw_image(s, {x, y, w, h}, "lead_car_dist_0", 0.3f);}
}

static void ui_draw_vision_brake(UIState *s) {
  const UIScene *scene = &s->scene;

  const int radius = 85;
  const int center_x = radius + bdr_s + radius*2 + 30;
  const int center_y = 1080 - 85 - 30;

  bool brake_valid = scene->car_state.getBrakeLights();
  float brake_img_alpha = brake_valid ? 1.0f : 0.15f;
  float brake_bg_alpha = brake_valid ? 0.3f : 0.1f;
  NVGcolor brake_bg = nvgRGBA(0, 0, 0, (255 * brake_bg_alpha));
  ui_draw_circle_image_rotation(s, center_x, center_y, radius, "brake", brake_bg, brake_img_alpha);
}

static void ui_draw_vision_autohold(UIState *s) {
  const UIScene *scene = &s->scene;
  int autohold = scene->car_state.getAutoHold();
  if(autohold < 0)
    return;

  const int radius = 85;
  const int center_x = radius + bdr_s + (radius*2 + 30) * 2;
  const int center_y = 1080 - 85 - 30;

  float brake_img_alpha = autohold > 0 ? 1.0f : 0.15f;
  float brake_bg_alpha = autohold > 0 ? 0.3f : 0.1f;
  NVGcolor brake_bg = nvgRGBA(0, 0, 0, (255 * brake_bg_alpha));

  ui_draw_circle_image_rotation(s, center_x, center_y, radius,
        autohold > 1 ? "autohold_warning" : "autohold_active",
        brake_bg, brake_img_alpha);
}

static void ui_draw_vision_maxspeed_org(UIState *s) {
  const int SET_SPEED_NA = 255;
  float maxspeed = s->scene.controls_state.getVCruise();
  float cruise_speed = s->scene.vSetDis;
  const bool is_cruise_set = maxspeed != 0 && maxspeed != SET_SPEED_NA;
  s->scene.is_speed_over_limit = s->scene.limitSpeedCamera > 29 && ((s->scene.limitSpeedCamera+round(s->scene.limitSpeedCamera*0.01*s->scene.speed_lim_off))+1 < s->scene.car_state.getVEgo()*3.6);
  if (is_cruise_set && !s->scene.is_metric) { maxspeed *= 0.6225; }

  const Rect rect = {bdr_s, bdr_s, 184, 202};
  NVGcolor color = COLOR_BLACK_ALPHA(100);
  if (s->scene.is_speed_over_limit) {
    color = COLOR_OCHRE_ALPHA(100);
  } else if (s->scene.limitSpeedCamera > 29 && !s->scene.is_speed_over_limit) {
    color = nvgRGBA(0, 120, 0, 100);
  } else if (s->scene.cruiseAccStatus) {
    color = nvgRGBA(0, 100, 200, 100);
  } else if (s->scene.controls_state.getEnabled()) {
    color = COLOR_WHITE_ALPHA(75);
  }
  ui_fill_rect(s->vg, rect, color, 30.);
  ui_draw_rect(s->vg, rect, COLOR_WHITE_ALPHA(100), 10, 20.);

  nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
  if (cruise_speed >= 30 && s->scene.controls_state.getEnabled()) {
    const std::string cruise_speed_str = std::to_string((int)std::nearbyint(cruise_speed));
    ui_draw_text(s, rect.centerX(), bdr_s+65, cruise_speed_str.c_str(), 26 * 2.8, COLOR_WHITE_ALPHA(is_cruise_set ? 200 : 100), "sans-bold");
  } else {
  	ui_draw_text(s, rect.centerX(), bdr_s+65, "-", 26 * 2.8, COLOR_WHITE_ALPHA(is_cruise_set ? 200 : 100), "sans-semibold");
  }
  if (is_cruise_set) {
    const std::string maxspeed_str = std::to_string((int)std::nearbyint(maxspeed));
    ui_draw_text(s, rect.centerX(), bdr_s+165, maxspeed_str.c_str(), 48 * 2.4, COLOR_WHITE, "sans-bold");
  } else {
    ui_draw_text(s, rect.centerX(), bdr_s+165, "-", 42 * 2.4, COLOR_WHITE_ALPHA(100), "sans-semibold");
  }
}

static void ui_draw_vision_maxspeed(UIState *s) {
  const int SET_SPEED_NA = 255;
  float maxspeed = (*s->sm)["controlsState"].getControlsState().getVCruise();
  const bool is_cruise_set = maxspeed != 0 && maxspeed != SET_SPEED_NA && s->scene.controls_state.getEnabled();
  if (is_cruise_set && !s->scene.is_metric) { maxspeed *= 0.6225; }

  int viz_max_o = 184; //offset value to move right
  const Rect rect = {bdr_s, bdr_s, 184+viz_max_o, 202};
  ui_fill_rect(s->vg, rect, COLOR_BLACK_ALPHA(100), 20.);
  ui_draw_rect(s->vg, rect, COLOR_WHITE_ALPHA(100), 10, 20.);

  nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
  ui_draw_text(s, rect.centerX()+viz_max_o/2, bdr_s+65, "설정속도", 26 * 2.2, COLOR_WHITE_ALPHA(is_cruise_set ? 200 : 100), "sans-regular");
  if (is_cruise_set) {
    const std::string maxspeed_str = std::to_string((int)std::nearbyint(maxspeed));
    ui_draw_text(s, rect.centerX()+viz_max_o/2, bdr_s+165, maxspeed_str.c_str(), 48 * 2.3, COLOR_WHITE, "sans-bold");
  } else {
    ui_draw_text(s, rect.centerX()+viz_max_o/2, bdr_s+165, "-", 42 * 2.3, COLOR_WHITE_ALPHA(100), "sans-semibold");
  }
}

static void ui_draw_vision_cruise_speed(UIState *s) {
  float cruise_speed = s->scene.vSetDis;
  if (!s->scene.is_metric) { cruise_speed *= 0.621371; }
  s->scene.is_speed_over_limit = s->scene.limitSpeedCamera > 29 && ((s->scene.limitSpeedCamera+round(s->scene.limitSpeedCamera*0.01*s->scene.speed_lim_off))+1 < s->scene.car_state.getVEgo()*3.6);
  const Rect rect = {bdr_s, bdr_s, 184, 202};

  NVGcolor color = COLOR_GREY;
  if (s->scene.brakePress && !s->scene.comma_stock_ui ) {
    color = nvgRGBA(183, 0, 0, 200);
  } else if (s->scene.is_speed_over_limit) {
    color = COLOR_OCHRE_ALPHA(200);
  } else if (s->scene.limitSpeedCamera > 29 && !s->scene.is_speed_over_limit) {
    color = nvgRGBA(0, 120, 0, 200);
  } else if (s->scene.cruiseAccStatus) {
    color = nvgRGBA(0, 100, 200, 200);
  } else if (s->scene.controls_state.getEnabled()) {
    color = COLOR_WHITE_ALPHA(75);
  }
  ui_fill_rect(s->vg, rect, color, 20.);
  ui_draw_rect(s->vg, rect, COLOR_WHITE_ALPHA(100), 10, 20.);

  nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
  if (s->scene.limitSpeedCamera > 29) {
    ui_draw_text(s, rect.centerX(), bdr_s+65, "제한속도", 26 * 2.2, COLOR_WHITE_ALPHA(s->scene.cruiseAccStatus ? 200 : 100), "sans-regular");
  } else {
    ui_draw_text(s, rect.centerX(), bdr_s+65, "크루즈", 26 * 2.2, COLOR_WHITE_ALPHA(s->scene.cruiseAccStatus ? 200 : 100), "sans-regular");
  }
  const std::string cruise_speed_str = std::to_string((int)std::nearbyint(cruise_speed));
  if (cruise_speed >= 30 && s->scene.controls_state.getEnabled()) {
    ui_draw_text(s, rect.centerX(), bdr_s+165, cruise_speed_str.c_str(), 48 * 2.3, COLOR_WHITE, "sans-bold");
  } else {
    ui_draw_text(s, rect.centerX(), bdr_s+165, "-", 42 * 2.3, COLOR_WHITE_ALPHA(100), "sans-semibold");
  }
}

static void ui_draw_vision_cameradist(UIState *s) { 
  const int SET_SPEED_NA = 255;
  float maxspeed = s->scene.controls_state.getVCruise();
  const bool is_cruise_set = maxspeed != 0 && maxspeed != SET_SPEED_NA && s->scene.controls_state.getEnabled();
  float cameradist = s->scene.liveMapData.opkrspeedlimitdist;
  float cameradistkm = cameradist / 1000;
  if (is_cruise_set && !s->scene.is_metric) { maxspeed *= 0.6225; }

  char str[64];
  snprintf(str, sizeof(str), "%.1f", (float)cameradistkm);

  const Rect rect = {(bdr_s) + 2 * (184 + 15), int((bdr_s)) + 200, 200, 100};
  NVGcolor box_color = COLOR_WHITE;
  NVGcolor box_line_color = COLOR_WHITE_ALPHA(100);
  NVGcolor text_color = COLOR_WHITE;

  if (s->scene.is_speed_over_limit) {
      if (float(int(cameradist)/s->scene.liveMapData.opkrspeedlimit) < 3.0 ){// 잔여거리가 단속속도보다 3배 이내이면
        box_color = nvgRGBA(180, 0, 0, 200);      
      } 
      else {
        box_color = COLOR_OCHRE_ALPHA(200);
      }
  } else if (s->scene.liveMapData.opkrspeedlimit > 29 && !s->scene.is_speed_over_limit) {
      box_color = nvgRGBA(0, 120, 0, 200);
  } else {
      box_color = COLOR_WHITE_ALPHA(0);
      box_line_color = COLOR_WHITE_ALPHA(0);
      text_color = COLOR_WHITE_ALPHA(0);
  }

  ui_fill_rect(s->vg, rect, box_color, 20.);
  ui_draw_rect(s->vg, rect, box_line_color, 5, 20.);
  nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE); 

  if (s->scene.liveMapData.opkrspeedlimitdist > 1000){
    //const std::string cameradist_str = std::to_string((int)std::nearbyint(cameradist));
    ui_draw_text(s, rect.centerX() - 20, int(bdr_s)+275, str, 40 * 2.0, text_color, "sans-bold");
    ui_draw_text(s, rect.centerX() + 65, int(bdr_s)+280, "km", 30 * 1.6, text_color, "sans-semibold");
  } else if (s->scene.liveMapData.opkrspeedlimit > 29){    
    const std::string cameradist_str = std::to_string((int)std::nearbyint(cameradist));
    ui_draw_text(s, rect.centerX() - 15, int(bdr_s)+275, cameradist_str.c_str(), 40 * 2.0, text_color, "sans-bold");
    ui_draw_text(s, rect.centerX() + 65, int(bdr_s)+280, "m", 30 * 1.6, text_color, "sans-semibold");
  } else {
    const std::string cameradist_str = std::to_string((int)std::nearbyint(cameradist));
    ui_draw_text(s, rect.centerX() - 15, int(bdr_s)+275, cameradist_str.c_str(), 36 * 2.0, text_color, "sans-semibold");
    ui_draw_text(s, rect.centerX() + 65, int(bdr_s)+280, "m", 26 * 1.6, text_color, "sans-semibold");
  } 
}

static void ui_draw_vision_speed(UIState *s) {
  const float speed = std::max(0.0, (*s->sm)["carState"].getCarState().getVEgo() * (s->scene.is_metric ? 3.6 : 2.2369363));
  const std::string speed_str = std::to_string((int)std::nearbyint(speed));
  UIScene &scene = s->scene;  
  const int viz_speed_w = 250;
  const int viz_speed_x = s->fb_w/2 - viz_speed_w/2;
  const int viz_add = 50;
  const int header_h = 400;  

  // turning blinker from kegman, moving signal by OPKR
  if ((scene.leftBlinker || scene.leftBlinker) && !scene.comma_stock_ui){
    scene.blinker_blinkingrate -= 5;
    if(scene.blinker_blinkingrate<0) scene.blinker_blinkingrate = 68; // blinker_blinkingrate 는 IG/FL 깜빡이 주기에 거의 맞춤

    float progress = (68 - scene.blinker_blinkingrate) / 68.0;
    float offset = progress * (6.4 - 1.0) + 1.0;
    if (offset < 1.0) offset = 1.0;
    if (offset > 6.4) offset = 6.4;

    float alpha = 1.0;
    if (progress < 0.25) alpha = progress / 0.25;
    if (progress > 0.75) alpha = 1.0 - ((progress - 0.75) / 0.25);

    if(scene.leftBlinker) {
      nvgBeginPath(s->vg);
      nvgMoveTo(s->vg, viz_speed_x - (viz_add*offset)                  , (header_h/4.2));
      nvgLineTo(s->vg, viz_speed_x - (viz_add*offset) - (viz_speed_w/2), (header_h/2.1));
      nvgLineTo(s->vg, viz_speed_x - (viz_add*offset)                  , (header_h/1.4));
      nvgClosePath(s->vg);
      nvgFillColor(s->vg, nvgRGBA(255,100,0,(scene.blinker_blinkingrate<=68 && scene.blinker_blinkingrate>=30)?180:0));
      nvgFill(s->vg);
    }
    if(scene.rightBlinker) {
      nvgBeginPath(s->vg);
      nvgMoveTo(s->vg, viz_speed_x + (viz_add*offset) + viz_speed_w      , (header_h/4.2));
      nvgLineTo(s->vg, viz_speed_x + (viz_add*offset) + (viz_speed_w*1.5), (header_h/2.1));
      nvgLineTo(s->vg, viz_speed_x + (viz_add*offset) + viz_speed_w      , (header_h/1.4));
      nvgClosePath(s->vg);
      nvgFillColor(s->vg, nvgRGBA(255,100,0,(scene.blinker_blinkingrate<=68 && scene.blinker_blinkingrate>=30)?180:0));
      nvgFill(s->vg);
    }    
  }

  NVGcolor val_color = COLOR_WHITE;

  if (scene.brakePress && !scene.comma_stock_ui) val_color = nvgRGBA(180, 0, 0, 200);
  else if (scene.brakeLights && !scene.comma_stock_ui) val_color = nvgRGBA(255, 100, 0, 200);
  nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
  ui_draw_text(s, s->fb_w/2, 210, speed_str.c_str(), 96 * 2.5, val_color, "sans-bold");
  ui_draw_text(s, s->fb_w/2, 290, s->scene.is_metric ? "km/h" : "mph", 36 * 2.5, COLOR_WHITE_ALPHA(200), "sans-regular");
}

static void ui_draw_vision_event(UIState *s) {
  const int viz_event_w = 220;
  const int viz_event_x = s->fb_w - (viz_event_w + bdr_s);
  const int viz_event_y = bdr_s;

  const int center_x = (bdr_s) + 2 * (184 + 15);
  const int center_y = int(bdr_s);

  if (!s->scene.comma_stock_ui){
    // 버스전용차로( 246 )일 경우
    if (s->scene.liveMapData.opkrspeedsign == 246) {ui_draw_image(s, {center_x, center_y, 200, 200}, "bus_only", 0.8f);} 
    // 차선변경금지( 198 || 199 || 249 )일 경우
    if (s->scene.mapSign == 198 || s->scene.mapSign == 199 || s->scene.mapSign == 249) {
      ui_draw_image(s, {center_x, center_y, 200, 200}, "do_not_change_lane", 0.8f);}
    // 구간단속구간( 165 )일 경우
    if (s->scene.mapSign == 165 && s->scene.liveMapData.opkrspeedlimit != 0) { 
      if (s->scene.liveMapData.opkrspeedlimit < 70) {ui_draw_image(s, {center_x, center_y, 200, 200}, "section_60", 0.8f);}
      else if (s->scene.liveMapData.opkrspeedlimit < 80) {ui_draw_image(s, {center_x, center_y, 200, 200}, "section_70", 0.8f);} 
      else if (s->scene.liveMapData.opkrspeedlimit < 90) {ui_draw_image(s, {center_x, center_y, 200, 200}, "section_80", 0.8f);}
      else if (s->scene.liveMapData.opkrspeedlimit < 100) {ui_draw_image(s, {center_x, center_y, 200, 200}, "section_90", 0.8f);}
      else if (s->scene.liveMapData.opkrspeedlimit < 110) {ui_draw_image(s, {center_x, center_y, 200, 200}, "section_100", 0.8f);}
      else if (s->scene.liveMapData.opkrspeedlimit < 120) {ui_draw_image(s, {center_x, center_y, 200, 200}, "section_110", 0.8f);}
    } 
    // 일반적인 과속단속구간( 135 || 150 || 200 || 231)일 경우  
    if ((s->scene.mapSign == 135 || s->scene.mapSign == 150 || s->scene.mapSign == 200 || s->scene.mapSign == 231) && s->scene.liveMapData.opkrspeedlimit > 29) {
      if (s->scene.liveMapData.opkrspeedlimit < 40) {ui_draw_image(s, {center_x, center_y, 200, 200}, "speed_30", 0.8f);
                                                    ui_draw_image(s, {960-200, 540+100, 400, 400}, "speed_S30", 0.2f);} //중앙 스쿨존 이미지
      else if (s->scene.liveMapData.opkrspeedlimit < 50) {ui_draw_image(s, {center_x, center_y, 200, 200}, "speed_40", 0.8f);} 
      else if (s->scene.liveMapData.opkrspeedlimit < 60) {ui_draw_image(s, {center_x, center_y, 200, 200}, "speed_50", 0.8f);}
      else if (s->scene.liveMapData.opkrspeedlimit < 70) {ui_draw_image(s, {center_x, center_y, 200, 200}, "speed_60", 0.8f);} 
      else if (s->scene.liveMapData.opkrspeedlimit < 80) {ui_draw_image(s, {center_x, center_y, 200, 200}, "speed_70", 0.8f);} 
      else if (s->scene.liveMapData.opkrspeedlimit < 90) {ui_draw_image(s, {center_x, center_y, 200, 200}, "speed_80", 0.8f);} 
      else if (s->scene.liveMapData.opkrspeedlimit < 100) {ui_draw_image(s, {center_x, center_y, 200, 200}, "speed_90", 0.8f);}
      else if (s->scene.liveMapData.opkrspeedlimit < 110) {ui_draw_image(s, {center_x, center_y, 200, 200}, "speed_100", 0.8f);}
      else if (s->scene.liveMapData.opkrspeedlimit < 120) {ui_draw_image(s, {center_x, center_y, 200, 200}, "speed_110", 0.8f);}
    }
    //가변구간( 195 || 197) 일 경우
    if (s->scene.mapSign == 195 || s->scene.mapSign == 197) {
      ui_draw_image(s, {center_x, center_y, 200, 200}, "speed_var", 0.8f); }
    //과속방지턱( 124 ) 일 경우
    if (s->scene.liveMapData.opkrspeedsign == 124) {
      ui_draw_image(s, {960-200, 540+50, 400, 400}, "speed_bump", 0.2f); }
  }

  //draw compass by opkr and re-designed by hoya
  if (s->scene.gpsAccuracyUblox != 0.00 && !s->scene.comma_stock_ui) {
    const int radius = 85;
    const int compass_x = 1920 / 2 - 20;
    const int compass_y = 1080 - 40;
    // const int compass_x = s->fb_w - 167 - bdr_s;
    // const int compass_y = bdr_s + 713;
    // const int direction_x = compass_x + 74;
    // const int direction_y = compass_y + 74;
    // ui_draw_circle_image_rotation(s, direction_x, direction_y - (bdr_s+7), 100, "direction", nvgRGBA(0x0, 0x0, 0x0, 0x0), 1.0f, -(s->scene.bearingUblox));
    // ui_draw_image(s, {compass_x, compass_y, 150, 150}, "compass", 1.0f);
    ui_draw_circle_image_rotation(s, compass_x, compass_y, radius + 40, "direction", nvgRGBA(0, 0, 0, 0), 0.7f, -(s->scene.bearingUblox));
    ui_draw_circle_image_rotation(s, compass_x, compass_y, radius + 40, "compass", nvgRGBA(0, 0, 0, 0), 0.8f);
  }

  // draw steering wheel
  const int bg_wheel_size = 90;
  const int bg_wheel_x = viz_event_x + (viz_event_w-bg_wheel_size);
  const int bg_wheel_y = viz_event_y + (bg_wheel_size/2);
  const QColor &color = bg_colors[s->status];
  NVGcolor nvg_color = nvgRGBA(color.red(), color.green(), color.blue(), color.alpha());
  if (s->scene.controls_state.getEnabled() || s->scene.forceGearD || s->scene.comma_stock_ui) {
    float angleSteers = s->scene.car_state.getSteeringAngleDeg();
    if (s->scene.controlAllowed) {
      ui_draw_circle_image_rotation(s, bg_wheel_x, bg_wheel_y+20, bg_wheel_size, "wheel", nvg_color, 1.0f, angleSteers);
    } else {
      ui_draw_circle_image_rotation(s, bg_wheel_x, bg_wheel_y+20, bg_wheel_size, "wheel", nvgRGBA(0x17, 0x33, 0x49, 0xc8), 1.0f, angleSteers);
    }
  } else {
    if (!s->scene.comma_stock_ui) ui_draw_gear(s);
  }
  if (!s->scene.comma_stock_ui) ui_draw_debug(s);
}

//BB START: functions added for the display of various items
static int bb_ui_draw_measure(UIState *s, const char* bb_value, const char* bb_uom, const char* bb_label,
    int bb_x, int bb_y, int bb_uom_dx,
    NVGcolor bb_valueColor, NVGcolor bb_labelColor, NVGcolor bb_uomColor,
    int bb_valueFontSize, int bb_labelFontSize, int bb_uomFontSize )  {
  nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
  int dx = 0;
  if (strlen(bb_uom) > 0) {
    dx = (int)(bb_uomFontSize*2.5/2);
   }
  //print value
  nvgFontFace(s->vg, "sans-semibold");
  nvgFontSize(s->vg, bb_valueFontSize*2.5);
  nvgFillColor(s->vg, bb_valueColor);
  nvgText(s->vg, bb_x-dx/2, bb_y+ (int)(bb_valueFontSize*2.5)+5, bb_value, NULL);
  //print label
  nvgFontFace(s->vg, "sans-regular");
  nvgFontSize(s->vg, bb_labelFontSize*2.5);
  nvgFillColor(s->vg, bb_labelColor);
  nvgText(s->vg, bb_x, bb_y + (int)(bb_valueFontSize*2.5)+5 + (int)(bb_labelFontSize*2.5)+5, bb_label, NULL);
  //print uom
  if (strlen(bb_uom) > 0) {
      nvgSave(s->vg);
    int rx =bb_x + bb_uom_dx + bb_valueFontSize -3;
    int ry = bb_y + (int)(bb_valueFontSize*2.5/2)+25;
    nvgTranslate(s->vg,rx,ry);
    nvgRotate(s->vg, -1.5708); //-90deg in radians
    nvgFontFace(s->vg, "sans-regular");
    nvgFontSize(s->vg, (int)(bb_uomFontSize*2.5));
    nvgFillColor(s->vg, bb_uomColor);
    nvgText(s->vg, 0, 0, bb_uom, NULL);
    nvgRestore(s->vg);
  }
  return (int)((bb_valueFontSize + bb_labelFontSize)*2.5) + 5;
}

static void bb_ui_draw_measures_left(UIState *s, int bb_x, int bb_y, int bb_w ) {
  const UIScene &scene = s->scene;
  int bb_rx = bb_x + (int)(bb_w/2);
  int bb_ry = bb_y;
  int bb_h = 5;
  NVGcolor lab_color = COLOR_WHITE_ALPHA(200);
  NVGcolor uom_color = COLOR_WHITE_ALPHA(200);
  int value_fontSize=30*0.8;
  int label_fontSize=15*0.8;
  int uom_fontSize = 15*0.8;
  int bb_uom_dx =  (int)(bb_w /2 - uom_fontSize*2.5) ;
  //CPU TEMP
  if (true) {
    //char val_str[16];
    char uom_str[6];
    std::string cpu_temp_val = std::to_string(int(scene.cpuTemp)) + "°C";
    NVGcolor val_color = COLOR_WHITE_ALPHA(200);
    if(scene.cpuTemp > 75) {
      val_color = nvgRGBA(255, 188, 3, 200);
    }
    if(scene.cpuTemp > 85) {
      val_color = nvgRGBA(255, 0, 0, 200);
    }
    //snprintf(val_str, sizeof(val_str), "%.0fC", (round(scene.cpuTemp)));
    snprintf(uom_str, sizeof(uom_str), "%d%%", (scene.cpuPerc));
    bb_h +=bb_ui_draw_measure(s, cpu_temp_val.c_str(), uom_str, "CPU 온도",
        bb_rx, bb_ry, bb_uom_dx,
        val_color, lab_color, uom_color,
        value_fontSize, label_fontSize, uom_fontSize );
    bb_ry = bb_y + bb_h;
  }
  //DEVICE TEMP
  if (scene.batt_less) {
    //char val_str[16];
    char uom_str[6];
    std::string device_temp_val = std::to_string(int(scene.ambientTemp)) + "°C";
    NVGcolor val_color = COLOR_WHITE_ALPHA(200);
    if(scene.ambientTemp > 45) {
      val_color = nvgRGBA(255, 188, 3, 200);
    }
    if(scene.ambientTemp > 50) {
      val_color = nvgRGBA(255, 0, 0, 200);
    }
    // temp is alway in C * 1000
    //snprintf(val_str, sizeof(val_str), "%.0fC", batteryTemp);
    snprintf(uom_str, sizeof(uom_str), "%d", (scene.fanSpeed)/1000);
    bb_h +=bb_ui_draw_measure(s, device_temp_val.c_str(), uom_str, "시스템온도",
        bb_rx, bb_ry, bb_uom_dx,
        val_color, lab_color, uom_color,
        value_fontSize, label_fontSize, uom_fontSize );
    bb_ry = bb_y + bb_h;
  }
  //BAT TEMP
  if (!scene.batt_less) {
    //char val_str[16];
    char uom_str[6];
    std::string bat_temp_val = std::to_string(int(scene.batTemp)) + "°C";
    NVGcolor val_color = COLOR_WHITE_ALPHA(200);
    if(scene.batTemp > 40) {
      val_color = nvgRGBA(255, 188, 3, 200);
    }
    if(scene.batTemp > 50) {
      val_color = nvgRGBA(255, 0, 0, 200);
    }
    // temp is alway in C * 1000
    //snprintf(val_str, sizeof(val_str), "%.0fC", batteryTemp);
    snprintf(uom_str, sizeof(uom_str), "%d", (scene.fanSpeed)/1000);
    bb_h +=bb_ui_draw_measure(s, bat_temp_val.c_str(), uom_str, "배터리온도",
        bb_rx, bb_ry, bb_uom_dx,
        val_color, lab_color, uom_color,
        value_fontSize, label_fontSize, uom_fontSize );
    bb_ry = bb_y + bb_h;
  }
  //BAT LEVEL
  if(!scene.batt_less) {
    //char val_str[16];
    char uom_str[6];
    std::string bat_level_val = std::to_string(int(scene.batPercent)) + "%";
    NVGcolor val_color = COLOR_WHITE_ALPHA(200);
    snprintf(uom_str, sizeof(uom_str), "%s", scene.deviceState.getBatteryStatus() == "Charging" ? "++" : "--");
    bb_h +=bb_ui_draw_measure(s, bat_level_val.c_str(), uom_str, "배터리레벨",
        bb_rx, bb_ry, bb_uom_dx,
        val_color, lab_color, uom_color,
        value_fontSize, label_fontSize, uom_fontSize );
    bb_ry = bb_y + bb_h;
  }
  //add Ublox GPS accuracy
  if (scene.gpsAccuracyUblox != 0.00) {
    char val_str[16];
    char uom_str[6];
    NVGcolor val_color = COLOR_WHITE_ALPHA(200);
    //show red/orange if gps accuracy is low
      if(scene.gpsAccuracyUblox > 0.85) {
         val_color = COLOR_ORANGE_ALPHA(200);
      }
      if(scene.gpsAccuracyUblox > 1.3) {
         val_color = COLOR_RED_ALPHA(200);
      }
    // gps accuracy is always in meters
    if(scene.gpsAccuracyUblox > 99 || scene.gpsAccuracyUblox == 0) {
       snprintf(val_str, sizeof(val_str), "None");
    }else if(scene.gpsAccuracyUblox > 9.99) {
      snprintf(val_str, sizeof(val_str), "%.1f", (scene.gpsAccuracyUblox));
    }
    else {
      snprintf(val_str, sizeof(val_str), "%.2f", (scene.gpsAccuracyUblox));
    }
    snprintf(uom_str, sizeof(uom_str), "%d", (scene.satelliteCount));
    bb_h +=bb_ui_draw_measure(s,  val_str, uom_str, "GPS 정확도",
        bb_rx, bb_ry, bb_uom_dx,
        val_color, lab_color, uom_color,
        value_fontSize, label_fontSize, uom_fontSize );
    bb_ry = bb_y + bb_h;
  }
  //add altitude
  if (scene.gpsAccuracyUblox != 0.00) {
    char val_str[16];
    char uom_str[6];
    NVGcolor val_color = COLOR_WHITE_ALPHA(200);
    snprintf(val_str, sizeof(val_str), "%.0f", (scene.altitudeUblox));
    snprintf(uom_str, sizeof(uom_str), "m");
    bb_h +=bb_ui_draw_measure(s,  val_str, uom_str, "고도",
        bb_rx, bb_ry, bb_uom_dx,
        val_color, lab_color, uom_color,
        value_fontSize, label_fontSize, uom_fontSize );
    bb_ry = bb_y + bb_h;
  }

  //finally draw the frame
  bb_h += 20;
  nvgBeginPath(s->vg);
  nvgRoundedRect(s->vg, bb_x, bb_y, bb_w, bb_h, 20);
  nvgStrokeColor(s->vg, COLOR_WHITE_ALPHA(80));
  nvgStrokeWidth(s->vg, 6);
  nvgStroke(s->vg);
}

static void bb_ui_draw_measures_right(UIState *s, int bb_x, int bb_y, int bb_w ) {
  const UIScene &scene = s->scene;
  int bb_rx = bb_x + (int)(bb_w/2);
  int bb_ry = bb_y;
  int bb_h = 5;
  NVGcolor lab_color = COLOR_WHITE_ALPHA(200);
  NVGcolor uom_color = COLOR_WHITE_ALPHA(200);
  int value_fontSize=30*0.8;
  int label_fontSize=15*0.8;
  int uom_fontSize = 15*0.8;
  int bb_uom_dx =  (int)(bb_w /2 - uom_fontSize*2.5);
  auto lead_one = (*s->sm)["modelV2"].getModelV2().getLeadsV3()[0];

  //add visual radar relative distance
  if (true) {
    char val_str[16];
    char uom_str[6];
    NVGcolor val_color = COLOR_WHITE_ALPHA(200);
    if (lead_one.getProb() > .5) {
      //show RED if less than 5 meters
      //show orange if less than 15 meters
      if((int)(lead_one.getX()[0] - 2.5) < 15) {
        val_color = COLOR_ORANGE_ALPHA(200);
      }
      if((int)(lead_one.getX()[0] - 2.5) < 5) {
        val_color = COLOR_RED_ALPHA(200);
      }
      // lead car relative distance is always in meters
      if((float)(lead_one.getX()[0] -2.5) < 10) {
        snprintf(val_str, sizeof(val_str), "%.1f", (float)(lead_one.getX()[0] - 2.5));
      } else {
        snprintf(val_str, sizeof(val_str), "%d", (int)(lead_one.getX()[0] - 2.5));
      }

    } else {
       snprintf(val_str, sizeof(val_str), "-");
    }
    snprintf(uom_str, sizeof(uom_str), "m");
    bb_h +=bb_ui_draw_measure(s,  val_str, uom_str, "차간거리",
        bb_rx, bb_ry, bb_uom_dx,
        val_color, lab_color, uom_color,
        value_fontSize, label_fontSize, uom_fontSize );
    bb_ry = bb_y + bb_h;
  }
  //add visual radar relative speed
  if (true) {
    char val_str[16];
    char uom_str[6];
    NVGcolor val_color = COLOR_WHITE_ALPHA(200);
    if (lead_one.getProb() > .5) {
      //show Orange if negative speed (approaching)
      //show Orange if negative speed faster than 5mph (approaching fast)
      if((int)((lead_one.getV()[0] - scene.car_state.getVEgoOP()) * 3.6) < 0) {
        val_color = nvgRGBA(255, 188, 3, 200);
      }
      if((int)((lead_one.getV()[0] - scene.car_state.getVEgoOP()) * 3.6) < -5) {
        val_color = nvgRGBA(255, 0, 0, 200);
      }
      // lead car relative speed is always in meters
      if (scene.is_metric) {
         snprintf(val_str, sizeof(val_str), "%d", (int)((lead_one.getV()[0] - scene.car_state.getVEgoOP()) * 3.6));
      } else {
         snprintf(val_str, sizeof(val_str), "%d", (int)((lead_one.getV()[0] - scene.car_state.getVEgoOP()) * 2.2374144));
      }
    } else {
       snprintf(val_str, sizeof(val_str), "-");
    }
    if (scene.is_metric) {
      snprintf(uom_str, sizeof(uom_str), "km/h");;
    } else {
      snprintf(uom_str, sizeof(uom_str), "mi/h");
    }
    bb_h +=bb_ui_draw_measure(s,  val_str, uom_str, "상대속도",
        bb_rx, bb_ry, bb_uom_dx,
        val_color, lab_color, uom_color,
        value_fontSize, label_fontSize, uom_fontSize );
    bb_ry = bb_y + bb_h;
  }
  //add steering angle
  if (true) {
    char val_str[16];
    char uom_str[6];
    //std::string angle_val = std::to_string(int(scene.angleSteers*10)/10) + "°";
    NVGcolor val_color = COLOR_GREEN_ALPHA(200);
    //show Orange if more than 30 degrees
    //show red if  more than 50 degrees
    if(((int)(scene.angleSteers) < -30) || ((int)(scene.angleSteers) > 30)) {
      val_color = COLOR_ORANGE_ALPHA(200);
    }
    if(((int)(scene.angleSteers) < -50) || ((int)(scene.angleSteers) > 50)) {
      val_color = COLOR_RED_ALPHA(200);
    }
    // steering is in degrees
    snprintf(val_str, sizeof(val_str), "%.1f°",(scene.angleSteers));
    snprintf(uom_str, sizeof(uom_str), "   °");

    bb_h +=bb_ui_draw_measure(s, val_str, uom_str, "현재조향각",
        bb_rx, bb_ry, bb_uom_dx,
        val_color, lab_color, uom_color,
        value_fontSize, label_fontSize, uom_fontSize );
    bb_ry = bb_y + bb_h;
  }

  //add steerratio from lateralplan
  if (true) {
    char val_str[16];
    char uom_str[6];
    NVGcolor val_color = COLOR_WHITE_ALPHA(200);
    if (scene.controls_state.getEnabled()) {
      snprintf(val_str, sizeof(val_str), "%.2f",(scene.steerRatio));
    } else {
       snprintf(val_str, sizeof(val_str), "-");
    }
    snprintf(uom_str, sizeof(uom_str), "");
    bb_h +=bb_ui_draw_measure(s,  val_str, uom_str, "SteerRatio",
        bb_rx, bb_ry, bb_uom_dx,
        val_color, lab_color, uom_color,
        value_fontSize, label_fontSize, uom_fontSize );
    bb_ry = bb_y + bb_h;
  }

  //cruise gap
  if (scene.longitudinal_control) {
    char val_str[16];
    char uom_str[6];
    NVGcolor val_color = COLOR_WHITE_ALPHA(200);
    if (scene.controls_state.getEnabled()) {
      if (scene.cruise_gap == scene.dynamic_tr_mode) {
        snprintf(val_str, sizeof(val_str), "AUT");
        snprintf(uom_str, sizeof(uom_str), "%.2f",(scene.dynamic_tr_value));
      } else {
        snprintf(val_str, sizeof(val_str), "%d",(scene.cruise_gap));
        snprintf(uom_str, sizeof(uom_str), "S");
      }
    } else {
      snprintf(val_str, sizeof(val_str), "-");
      snprintf(uom_str, sizeof(uom_str), "");
    }
    bb_h +=bb_ui_draw_measure(s,  val_str, uom_str, "크루즈갭",
        bb_rx, bb_ry, bb_uom_dx,
        val_color, lab_color, uom_color,
        value_fontSize, label_fontSize, uom_fontSize );
    bb_ry = bb_y + bb_h;
  }

  //finally draw the frame
  bb_h += 20;
  nvgBeginPath(s->vg);
  nvgRoundedRect(s->vg, bb_x, bb_y, bb_w, bb_h, 20);
  nvgStrokeColor(s->vg, COLOR_WHITE_ALPHA(80));
  nvgStrokeWidth(s->vg, 6);
  nvgStroke(s->vg);
}

//BB END: functions added for the display of various items

static void bb_ui_draw_UI(UIState *s) {
  const int bb_dml_w = 180;
  const int bb_dml_x = bdr_s;
  const int bb_dml_y = bdr_s + 220;

  const int bb_dmr_w = 180;
  const int bb_dmr_x = s->fb_w - bb_dmr_w - bdr_s;
  const int bb_dmr_y = bdr_s + 220;

  bb_ui_draw_measures_right(s, bb_dml_x, bb_dml_y, bb_dml_w);
  bb_ui_draw_measures_left(s, bb_dmr_x, bb_dmr_y-20, bb_dmr_w);
}

static void draw_navi_button(UIState *s) {
  if (s->vipc_client->connected || s->scene.is_OpenpilotViewEnabled) {
    int btn_w = 140;
    int btn_h = 140;
    int btn_x1 = s->fb_w - btn_w - 355 - 40;
    int btn_y = 1080 - btn_h - 30;
    int btn_xc1 = btn_x1 + (btn_w/2);
    int btn_yc = btn_y + (btn_h/2);
    nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgBeginPath(s->vg);
    nvgRoundedRect(s->vg, btn_x1, btn_y, btn_w, btn_h, 100);
    nvgStrokeColor(s->vg, nvgRGBA(0,160,200,255));
    nvgStrokeWidth(s->vg, 6);
    nvgStroke(s->vg);
    nvgFontSize(s->vg, 45);
    if (s->scene.map_is_running) {
      NVGcolor fillColor = nvgRGBA(0,160,200,80);
      nvgFillColor(s->vg, fillColor);
      nvgFill(s->vg);
    }
    nvgFillColor(s->vg, nvgRGBA(255,255,255,200));
    nvgText(s->vg,btn_xc1,btn_yc,"NAVI",NULL);
  }
}

static void draw_laneless_button(UIState *s) {
  if (s->vipc_client->connected || s->scene.is_OpenpilotViewEnabled) {
    int btn_w = 140;
    int btn_h = 140;
    int btn_x1 = s->fb_w - btn_w - 195 - 20;
    int btn_y = 1080 - btn_h - 30;
    int btn_xc1 = btn_x1 + (btn_w/2);
    int btn_yc = btn_y + (btn_h/2);
    nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgBeginPath(s->vg);
    nvgRoundedRect(s->vg, btn_x1, btn_y, btn_w, btn_h, 100);
    nvgStrokeColor(s->vg, nvgRGBA(0,0,0,80));
    nvgStrokeWidth(s->vg, 6);
    nvgStroke(s->vg);
    nvgFontSize(s->vg, 43);    
    
    if (s->scene.laneless_mode == 0) {
      nvgStrokeColor(s->vg, nvgRGBA(0,125,0,255));
      nvgStrokeWidth(s->vg, 6);
      nvgStroke(s->vg);
      NVGcolor fillColor = nvgRGBA(0,125,0,80);
      nvgFillColor(s->vg, fillColor);
      nvgFill(s->vg);     
      nvgFillColor(s->vg, nvgRGBA(255,255,255,200));       
      nvgText(s->vg,btn_xc1,btn_yc-20,"Lane",NULL);
      nvgText(s->vg,btn_xc1,btn_yc+20,"only",NULL);
    } else if (s->scene.laneless_mode == 1) {
      nvgStrokeColor(s->vg, nvgRGBA(0,100,255,255));
      nvgStrokeWidth(s->vg, 6);
      nvgStroke(s->vg);
      NVGcolor fillColor = nvgRGBA(0,100,255,80);
      nvgFillColor(s->vg, fillColor);
      nvgFill(s->vg);      
      nvgFillColor(s->vg, nvgRGBA(255,255,255,200));
      nvgText(s->vg,btn_xc1,btn_yc-20,"Lane",NULL);
      nvgText(s->vg,btn_xc1,btn_yc+20,"-less",NULL);      
    } else if (s->scene.laneless_mode == 2) {
      nvgStrokeColor(s->vg, nvgRGBA(125,0,125,255));
      nvgStrokeWidth(s->vg, 6);
      nvgStroke(s->vg);
      NVGcolor fillColor = nvgRGBA(125,0,125,80);
      nvgFillColor(s->vg, fillColor);
      nvgFill(s->vg);
      nvgFillColor(s->vg, nvgRGBA(255,255,255,200));
      nvgText(s->vg,btn_xc1,btn_yc-20,"Auto",NULL);
      nvgText(s->vg,btn_xc1,btn_yc+20,"Lane",NULL);
    }
  }
}

static void ui_draw_vision_header(UIState *s) {
  NVGpaint gradient = nvgLinearGradient(s->vg, 0, header_h - (header_h / 2.5), 0, header_h,
                                        nvgRGBAf(0, 0, 0, 0.45), nvgRGBAf(0, 0, 0, 0));
  ui_fill_rect(s->vg, {0, 0, s->fb_w , header_h}, gradient);

  ui_draw_vision_speed(s); //중앙속도 & 깜빡이
  ui_draw_vision_event(s); //과속카메라 속도

  if (!s->scene.comma_stock_ui) {
    ui_draw_vision_cameradist(s); //과속카메라 거리 
    ui_draw_vision_maxspeed(s);
    ui_draw_vision_cruise_speed(s);
  } else {
    ui_draw_vision_maxspeed_org(s);
  }

 
  if (!s->scene.comma_stock_ui) {
    bb_ui_draw_UI(s);
    ui_draw_tpms(s);
    draw_navi_button(s);
  }
  if (s->scene.end_to_end && !s->scene.comma_stock_ui) {
    draw_laneless_button(s);
  }
  if (s->scene.controls_state.getEnabled() && !s->scene.comma_stock_ui) {
    ui_draw_standstill(s);
  }
}

//blind spot warning by OPKR
static void ui_draw_vision_car(UIState *s) {
  UIScene &scene = s->scene;
  const int car_size = 350;
  const int car_x_left = (s->fb_w/2 - 400);
  const int car_x_right = (s->fb_w/2 + 400);
  const int car_y = 500;
  const int car_img_size_w = (car_size * 1);
  const int car_img_size_h = (car_size * 1);
  const int car_img_x_left = (car_x_left - (car_img_size_w / 2));
  const int car_img_x_right = (car_x_right - (car_img_size_w / 2));
  const int car_img_y = (car_y - (car_size / 4) + 150);

  int car_valid_status = 0;
  bool car_valid_left = scene.leftblindspot;
  bool car_valid_right = scene.rightblindspot;
  float car_img_alpha;
  if (scene.nOpkrBlindSpotDetect) {
    if (scene.car_valid_status_changed != car_valid_status) {
      scene.blindspot_blinkingrate = 114;
      scene.car_valid_status_changed = car_valid_status;
    }
    if (car_valid_left || car_valid_right) {
      if (!car_valid_left && car_valid_right) {
        car_valid_status = 1;
      } else if (car_valid_left && !car_valid_right) {
        car_valid_status = 2;
      } else if (car_valid_left && car_valid_right) {
        car_valid_status = 3;
      } else {
        car_valid_status = 0;
      }
      scene.blindspot_blinkingrate -= 6;
      if(scene.blindspot_blinkingrate<0) scene.blindspot_blinkingrate = 120;
      if (scene.blindspot_blinkingrate>=60) {
        car_img_alpha = 0.6f;
      } else {
        car_img_alpha = 0.0f;
      }
    } else {
      scene.blindspot_blinkingrate = 120;
    }

    if(car_valid_left) {
      ui_draw_image(s, {car_img_x_left, car_img_y, car_img_size_w, car_img_size_h}, "car_left", car_img_alpha);
    }
    if(car_valid_right) {
      ui_draw_image(s, {car_img_x_right, car_img_y, car_img_size_w, car_img_size_h}, "car_right", car_img_alpha);
    }
  }
}

static void ui_draw_vision_footer(UIState *s) {
  ui_draw_vision_face(s);
  ui_draw_vision_scc_gap(s);
  #if UI_FEATURE_BRAKE
    ui_draw_vision_brake(s);
  #endif  
  #if UI_FEATURE_AUTOHOLD
    ui_draw_vision_autohold(s);
  #endif
}

// draw date/time
void draw_kr_date_time(UIState *s) {
  int rect_w = 600;
  const int rect_h = 50;
  int rect_x = s->fb_w/2 - rect_w/2;
  const int rect_y = 0;
  char dayofweek[50];

  // Get local time to display
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  char now[50];
  if (tm.tm_wday == 0) {
    strcpy(dayofweek, "SUN");
  } else if (tm.tm_wday == 1) {
    strcpy(dayofweek, "MON");
  } else if (tm.tm_wday == 2) {
    strcpy(dayofweek, "TUE");
  } else if (tm.tm_wday == 3) {
    strcpy(dayofweek, "WED");
  } else if (tm.tm_wday == 4) {
    strcpy(dayofweek, "THU");
  } else if (tm.tm_wday == 5) {
    strcpy(dayofweek, "FRI");
  } else if (tm.tm_wday == 6) {
    strcpy(dayofweek, "SAT");
  }

  if (s->scene.kr_date_show && s->scene.kr_time_show) {
    snprintf(now,sizeof(now),"%04d-%02d-%02d %s %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, dayofweek, tm.tm_hour, tm.tm_min, tm.tm_sec);
  } else if (s->scene.kr_date_show) {
    snprintf(now,sizeof(now),"%04d-%02d-%02d %s", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, dayofweek);
  } else if (s->scene.kr_time_show) {
    snprintf(now,sizeof(now),"%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
  }

  nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
  nvgBeginPath(s->vg);
  nvgRoundedRect(s->vg, rect_x, rect_y, rect_w, rect_h, 0);
  nvgFillColor(s->vg, nvgRGBA(0, 0, 0, 0));
  nvgFill(s->vg);
  nvgStrokeColor(s->vg, nvgRGBA(255,255,255,0));
  nvgStrokeWidth(s->vg, 0);
  nvgStroke(s->vg);

  nvgFontSize(s->vg, 50);
  nvgFillColor(s->vg, nvgRGBA(255, 255, 255, 200));
  nvgText(s->vg, s->fb_w/2, rect_y, now, NULL);
}

// live camera offset adjust by OPKR
static void ui_draw_live_tune_panel(UIState *s) {
  const int width = 160;
  const int height = 160;
  const int x_start_pos_l = s->fb_w/2 - width*2;
  const int x_start_pos_r = s->fb_w/2 + width*2;
  const int y_pos = 750;
  //left symbol_above
  nvgBeginPath(s->vg);
  nvgMoveTo(s->vg, x_start_pos_l, y_pos - 175);
  nvgLineTo(s->vg, x_start_pos_l - width + 30, y_pos + height/2 - 175);
  nvgLineTo(s->vg, x_start_pos_l, y_pos + height - 175);
  nvgClosePath(s->vg);
  nvgFillColor(s->vg, nvgRGBA(255,153,153,150));
  nvgFill(s->vg);
  //right symbol above
  nvgBeginPath(s->vg);
  nvgMoveTo(s->vg, x_start_pos_r, y_pos - 175);
  nvgLineTo(s->vg, x_start_pos_r + width - 30, y_pos + height/2 - 175);
  nvgLineTo(s->vg, x_start_pos_r, y_pos + height - 175);
  nvgClosePath(s->vg);
  nvgFillColor(s->vg, nvgRGBA(255,153,153,150));
  nvgFill(s->vg);
  //left symbol
  nvgBeginPath(s->vg);
  nvgMoveTo(s->vg, x_start_pos_l, y_pos);
  nvgLineTo(s->vg, x_start_pos_l - width + 30, y_pos + height/2);
  nvgLineTo(s->vg, x_start_pos_l, y_pos + height);
  nvgClosePath(s->vg);
  nvgFillColor(s->vg, nvgRGBA(171,242,0,150));
  nvgFill(s->vg);
  //right symbol
  nvgBeginPath(s->vg);
  nvgMoveTo(s->vg, x_start_pos_r, y_pos);
  nvgLineTo(s->vg, x_start_pos_r + width - 30, y_pos + height/2);
  nvgLineTo(s->vg, x_start_pos_r, y_pos + height);
  nvgClosePath(s->vg);

  nvgFillColor(s->vg, COLOR_WHITE_ALPHA(150));
  nvgFill(s->vg);

  //param value
  nvgFontSize(s->vg, 150);
  nvgTextAlign(s->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  if (s->scene.live_tune_panel_list == 0) {
    ui_print(s, s->fb_w/2, y_pos + height/2, "%+0.3f", s->scene.cameraOffset*0.001);
    nvgFontSize(s->vg, 75);
    ui_print(s, s->fb_w/2, y_pos - 95, "CameraOffset");
  } else if (s->scene.live_tune_panel_list == 1) {
    ui_print(s, s->fb_w/2, y_pos + height/2, "%+0.3f", s->scene.pathOffset*0.001);
    nvgFontSize(s->vg, 75);
    ui_print(s, s->fb_w/2, y_pos - 95, "PathOffset");
  } else if (s->scene.live_tune_panel_list == 2) {
    ui_print(s, s->fb_w/2, y_pos + height/2, "%0.2f", s->scene.osteerRateCost*0.01);
    nvgFontSize(s->vg, 75);
    ui_print(s, s->fb_w/2, y_pos - 95, "SteerRateCost");
  } else if (s->scene.live_tune_panel_list == (s->scene.list_count+0) && s->scene.lateralControlMethod == 0) {
    ui_print(s, s->fb_w/2, y_pos + height/2, "%0.2f", s->scene.pidKp*0.01);
    nvgFontSize(s->vg, 75);
    ui_print(s, s->fb_w/2, y_pos - 95, "Pid: Kp");
  } else if (s->scene.live_tune_panel_list == (s->scene.list_count+1) && s->scene.lateralControlMethod == 0) {
    ui_print(s, s->fb_w/2, y_pos + height/2, "%0.3f", s->scene.pidKi*0.001);
    nvgFontSize(s->vg, 75);
    ui_print(s, s->fb_w/2, y_pos - 95, "Pid: Ki");
  } else if (s->scene.live_tune_panel_list == (s->scene.list_count+2) && s->scene.lateralControlMethod == 0) {
    ui_print(s, s->fb_w/2, y_pos + height/2, "%0.2f", s->scene.pidKd*0.01);
    nvgFontSize(s->vg, 75);
    ui_print(s, s->fb_w/2, y_pos - 95, "Pid: Kd");
  } else if (s->scene.live_tune_panel_list == (s->scene.list_count+3) && s->scene.lateralControlMethod == 0) {
    ui_print(s, s->fb_w/2, y_pos + height/2, "%0.5f", s->scene.pidKf*0.00001);
    nvgFontSize(s->vg, 75);
    ui_print(s, s->fb_w/2, y_pos - 95, "Pid: Kf");
  } else if (s->scene.live_tune_panel_list == (s->scene.list_count+0) && s->scene.lateralControlMethod == 1) {
    ui_print(s, s->fb_w/2, y_pos + height/2, "%0.1f", s->scene.indiInnerLoopGain*0.1);
    nvgFontSize(s->vg, 75);
    ui_print(s, s->fb_w/2, y_pos - 95, "INDI: ILGain");
  } else if (s->scene.live_tune_panel_list == (s->scene.list_count+1) && s->scene.lateralControlMethod == 1) {
    ui_print(s, s->fb_w/2, y_pos + height/2, "%0.1f", s->scene.indiOuterLoopGain*0.1);
    nvgFontSize(s->vg, 75);
    ui_print(s, s->fb_w/2, y_pos - 95, "INDI: OLGain");
  } else if (s->scene.live_tune_panel_list == (s->scene.list_count+2) && s->scene.lateralControlMethod == 1) {
    ui_print(s, s->fb_w/2, y_pos + height/2, "%0.1f", s->scene.indiTimeConstant*0.1);
    nvgFontSize(s->vg, 75);
    ui_print(s, s->fb_w/2, y_pos - 95, "INDI: TConst");
  } else if (s->scene.live_tune_panel_list == (s->scene.list_count+3) && s->scene.lateralControlMethod == 1) {
    ui_print(s, s->fb_w/2, y_pos + height/2, "%0.1f", s->scene.indiActuatorEffectiveness*0.1);
    nvgFontSize(s->vg, 75);
    ui_print(s, s->fb_w/2, y_pos - 95, "INDI: ActEffct");
  } else if (s->scene.live_tune_panel_list == (s->scene.list_count+0) && s->scene.lateralControlMethod == 2) {
    ui_print(s, s->fb_w/2, y_pos + height/2, "%0.0f", s->scene.lqrScale*1.0);
    nvgFontSize(s->vg, 75);
    ui_print(s, s->fb_w/2, y_pos - 95, "LQR: Scale");
  } else if (s->scene.live_tune_panel_list == (s->scene.list_count+1) && s->scene.lateralControlMethod == 2) {
    ui_print(s, s->fb_w/2, y_pos + height/2, "%0.3f", s->scene.lqrKi*0.001);
    nvgFontSize(s->vg, 75);
    ui_print(s, s->fb_w/2, y_pos - 95, "LQR: Ki");
  } else if (s->scene.live_tune_panel_list == (s->scene.list_count+2) && s->scene.lateralControlMethod == 2) {
    ui_print(s, s->fb_w/2, y_pos + height/2, "%0.5f", s->scene.lqrDcGain*0.00001);
    nvgFontSize(s->vg, 75);
    ui_print(s, s->fb_w/2, y_pos - 95, "LQR: DcGain");
  }
  nvgFillColor(s->vg, nvgRGBA(171,242,0,150));
  nvgFill(s->vg);
}

static void ui_draw_vision(UIState *s) {
  const UIScene *scene = &s->scene;
  // Draw augmented elements
  if (scene->world_objects_visible) {
    ui_draw_world(s);
  }
  // Set Speed, Current Speed, Status/Events
  ui_draw_vision_header(s);
  if ((*s->sm)["controlsState"].getControlsState().getAlertSize() == cereal::ControlsState::AlertSize::NONE && !scene->comma_stock_ui) {
    ui_draw_vision_footer(s);
    ui_draw_vision_car(s);
  }
  if (scene->live_tune_panel_enable) {
    ui_draw_live_tune_panel(s);
  }
  if ((scene->kr_date_show || scene->kr_time_show) && !scene->comma_stock_ui) {
    draw_kr_date_time(s);
  }
}

void ui_draw(UIState *s, int w, int h) {
  const bool draw_vision = s->scene.started && s->vipc_client->connected;

  glViewport(0, 0, s->fb_w, s->fb_h);
  if (draw_vision) {
    draw_vision_frame(s);
  }
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  // NVG drawing functions - should be no GL inside NVG frame
  nvgBeginFrame(s->vg, s->fb_w, s->fb_h, 1.0f);
  if (draw_vision) {
    ui_draw_vision(s);
    dashcam(s);
  }
  nvgEndFrame(s->vg);
  glDisable(GL_BLEND);
}

void ui_draw_image(const UIState *s, const Rect &r, const char *name, float alpha) {
  nvgBeginPath(s->vg);
  NVGpaint imgPaint = nvgImagePattern(s->vg, r.x, r.y, r.w, r.h, 0, s->images.at(name), alpha);
  nvgRect(s->vg, r.x, r.y, r.w, r.h);
  nvgFillPaint(s->vg, imgPaint);
  nvgFill(s->vg);
}

void ui_draw_rect(NVGcontext *vg, const Rect &r, NVGcolor color, int width, float radius) {
  nvgBeginPath(vg);
  radius > 0 ? nvgRoundedRect(vg, r.x, r.y, r.w, r.h, radius) : nvgRect(vg, r.x, r.y, r.w, r.h);
  nvgStrokeColor(vg, color);
  nvgStrokeWidth(vg, width);
  nvgStroke(vg);
}

static inline void fill_rect(NVGcontext *vg, const Rect &r, const NVGcolor *color, const NVGpaint *paint, float radius) {
  nvgBeginPath(vg);
  radius > 0 ? nvgRoundedRect(vg, r.x, r.y, r.w, r.h, radius) : nvgRect(vg, r.x, r.y, r.w, r.h);
  if (color) nvgFillColor(vg, *color);
  if (paint) nvgFillPaint(vg, *paint);
  nvgFill(vg);
}
void ui_fill_rect(NVGcontext *vg, const Rect &r, const NVGcolor &color, float radius) {
  fill_rect(vg, r, &color, nullptr, radius);
}
void ui_fill_rect(NVGcontext *vg, const Rect &r, const NVGpaint &paint, float radius) {
  fill_rect(vg, r, nullptr, &paint, radius);
}

static const char frame_vertex_shader[] =
#ifdef NANOVG_GL3_IMPLEMENTATION
  "#version 150 core\n"
#else
  "#version 300 es\n"
#endif
  "in vec4 aPosition;\n"
  "in vec4 aTexCoord;\n"
  "uniform mat4 uTransform;\n"
  "out vec4 vTexCoord;\n"
  "void main() {\n"
  "  gl_Position = uTransform * aPosition;\n"
  "  vTexCoord = aTexCoord;\n"
  "}\n";

static const char frame_fragment_shader[] =
#ifdef NANOVG_GL3_IMPLEMENTATION
  "#version 150 core\n"
#else
  "#version 300 es\n"
#endif
  "precision mediump float;\n"
  "uniform sampler2D uTexture;\n"
  "in vec4 vTexCoord;\n"
  "out vec4 colorOut;\n"
  "void main() {\n"
  "  colorOut = texture(uTexture, vTexCoord.xy);\n"
#ifdef QCOM
  "  vec3 dz = vec3(0.0627f, 0.0627f, 0.0627f);\n"
  "  colorOut.rgb = ((vec3(1.0f, 1.0f, 1.0f) - dz) * colorOut.rgb / vec3(1.0f, 1.0f, 1.0f)) + dz;\n"
#endif
  "}\n";

static const mat4 device_transform = {{
  1.0,  0.0, 0.0, 0.0,
  0.0,  1.0, 0.0, 0.0,
  0.0,  0.0, 1.0, 0.0,
  0.0,  0.0, 0.0, 1.0,
}};

void ui_nvg_init(UIState *s) {
  // init drawing

  // on EON, we enable MSAA
  s->vg = Hardware::EON() ? nvgCreate(0) : nvgCreate(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
  assert(s->vg);

  // init fonts
  std::pair<const char *, const char *> fonts[] = {
      {"sans-regular", "../assets/fonts/opensans_regular.ttf"},
      {"sans-semibold", "../assets/fonts/opensans_semibold.ttf"},
      {"sans-bold", "../assets/fonts/opensans_bold.ttf"},
  };
  for (auto [name, file] : fonts) {
    int font_id = nvgCreateFont(s->vg, name, file);
    assert(font_id >= 0);
  }

  // init images
  std::vector<std::pair<const char *, const char *>> images = {
    {"wheel", "../assets/img_chffr_wheel.png"},
    {"driver_face", "../assets/img_driver_face.png"},
    {"speed_S30", "../assets/addon/img/img_S30_speedahead.png"},    
    {"speed_30", "../assets/addon/img/img_30_speedahead.png"},
    {"speed_40", "../assets/addon/img/img_40_speedahead.png"},
    {"speed_50", "../assets/addon/img/img_50_speedahead.png"},
    {"speed_60", "../assets/addon/img/img_60_speedahead.png"},
    {"speed_70", "../assets/addon/img/img_70_speedahead.png"},
    {"speed_80", "../assets/addon/img/img_80_speedahead.png"},
    {"speed_90", "../assets/addon/img/img_90_speedahead.png"},
    {"speed_100", "../assets/addon/img/img_100_speedahead.png"},
    {"speed_110", "../assets/addon/img/img_110_speedahead.png"},
    {"section_60", "..//assets/addon/img/img_60_section.png"},
    {"section_70", "..//assets/addon/img/img_70_section.png"},
    {"section_80", "..//assets/addon/img/img_80_section.png"},
    {"section_90", "..//assets/addon/img/img_90_section.png"},
    {"section_100", "..//assets/addon/img/img_100_section.png"},
    {"section_110", "..//assets/addon/img/img_110_section.png"},    
    {"speed_var", "../assets/addon/img/img_var_speedahead.png"},
    {"speed_bump", "../assets/addon/img/img_speed_bump.png"},
    {"bus_only", "../assets/addon/img/img_bus_only.png"},
    {"do_not_change_lane", "../assets/addon/img/do_not_change_lane.png"},
    {"car_left", "../assets/addon/img/img_car_left.png"},
    {"car_right", "../assets/addon/img/img_car_right.png"},
    {"compass", "../assets/addon/img/img_compass.png"},
    {"direction", "../assets/addon/img/img_direction.png"},
    {"brake", "../assets/addon/img/img_brake_disc.png"},      
    {"autohold_warning", "../assets/addon/img/img_autohold_warning.png"},
    {"autohold_active", "../assets/addon/img/img_autohold_active.png"}, 
    {"lead_car_dist_0", "../assets/addon/img/car_dist_0.png"},
    {"lead_car_dist_1", "../assets/addon/img/car_dist_1.png"},    
    {"lead_car_dist_2", "../assets/addon/img/car_dist_2.png"},
    {"lead_car_dist_3", "../assets/addon/img/car_dist_3.png"},
    {"lead_car_dist_4", "../assets/addon/img/car_dist_4.png"},
    {"custom_lead_vision", "../assets/addon/img/custom_lead_vision.png"},
    {"custom_lead_radar", "../assets/addon/img/custom_lead_radar.png"},
    {"lead_radar", "../assets/addon/img/lead_radar.png"},
    {"lead_under_radar", "../assets/addon/img/lead_underline_radar.png"},
    {"lead_under_camera", "../assets/addon/img/lead_underline_camera.png"},
    {"tire_pressure", "../assets/images/img_tire_pressure.png"},
  };
  for (auto [name, file] : images) {
    s->images[name] = nvgCreateImage(s->vg, file, 1);
    assert(s->images[name] != 0);
  }

  // init gl
  s->gl_shader = std::make_unique<GLShader>(frame_vertex_shader, frame_fragment_shader);
  GLint frame_pos_loc = glGetAttribLocation(s->gl_shader->prog, "aPosition");
  GLint frame_texcoord_loc = glGetAttribLocation(s->gl_shader->prog, "aTexCoord");

  glViewport(0, 0, s->fb_w, s->fb_h);

  glDisable(GL_DEPTH_TEST);

  assert(glGetError() == GL_NO_ERROR);

  float x1 = 1.0, x2 = 0.0, y1 = 1.0, y2 = 0.0;
  const uint8_t frame_indicies[] = {0, 1, 2, 0, 2, 3};
  const float frame_coords[4][4] = {
    {-1.0, -1.0, x2, y1}, //bl
    {-1.0,  1.0, x2, y2}, //tl
    { 1.0,  1.0, x1, y2}, //tr
    { 1.0, -1.0, x1, y1}, //br
  };

  glGenVertexArrays(1, &s->frame_vao);
  glBindVertexArray(s->frame_vao);
  glGenBuffers(1, &s->frame_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, s->frame_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(frame_coords), frame_coords, GL_STATIC_DRAW);
  glEnableVertexAttribArray(frame_pos_loc);
  glVertexAttribPointer(frame_pos_loc, 2, GL_FLOAT, GL_FALSE,
                        sizeof(frame_coords[0]), (const void *)0);
  glEnableVertexAttribArray(frame_texcoord_loc);
  glVertexAttribPointer(frame_texcoord_loc, 2, GL_FLOAT, GL_FALSE,
                        sizeof(frame_coords[0]), (const void *)(sizeof(float) * 2));
  glGenBuffers(1, &s->frame_ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s->frame_ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(frame_indicies), frame_indicies, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  ui_resize(s, s->fb_w, s->fb_h);
}

void ui_resize(UIState *s, int width, int height) {
  s->fb_w = width;
  s->fb_h = height;

  auto intrinsic_matrix = s->wide_camera ? ecam_intrinsic_matrix : fcam_intrinsic_matrix;

  float zoom = ZOOM / intrinsic_matrix.v[0];

  if (s->wide_camera) {
    zoom *= 0.5;
  }

  float zx = zoom * 2 * intrinsic_matrix.v[2] / width;
  float zy = zoom * 2 * intrinsic_matrix.v[5] / height;

  const mat4 frame_transform = {{
    zx, 0.0, 0.0, 0.0,
    0.0, zy, 0.0, -y_offset / height * 2,
    0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 1.0,
  }};

  s->rear_frame_mat = matmul(device_transform, frame_transform);

  // Apply transformation such that video pixel coordinates match video
  // 1) Put (0, 0) in the middle of the video
  nvgTranslate(s->vg, width / 2, height / 2 + y_offset);
  // 2) Apply same scaling as video
  nvgScale(s->vg, zoom, zoom);
  // 3) Put (0, 0) in top left corner of video
  nvgTranslate(s->vg, -intrinsic_matrix.v[2], -intrinsic_matrix.v[5]);

  nvgCurrentTransform(s->vg, s->car_space_transform);
  nvgResetTransform(s->vg);
}
