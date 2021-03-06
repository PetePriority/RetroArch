/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <file/file_path.h>
#include "menu.h"
#include "menu_entries_cbs.h"
#include "menu_setting.h"
#include "menu_input.h"
#include "menu_entries.h"
#include "menu_shader.h"
#include "menu_navigation.h"

#include "../retroarch.h"

#include "../input/input_autodetect.h"

static int archive_open(void)
{
   char cat_path[PATH_MAX_LENGTH];
   const char *menu_path  = NULL;
   const char *menu_label = NULL;
   const char* path       = NULL;
   unsigned int type = 0;
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return -1;

   menu_list_pop_stack(menu->menu_list);

   menu_list_get_last_stack(menu->menu_list,
         &menu_path, &menu_label, NULL);

   if (menu_list_get_size(menu->menu_list) == 0)
      return 0;

   menu_list_get_at_offset(menu->menu_list->selection_buf,
         menu->navigation.selection_ptr, &path, NULL, &type);

   fill_pathname_join(cat_path, menu_path, path, sizeof(cat_path));
   menu_list_push_stack_refresh(
         menu->menu_list,
         cat_path,
         menu_label,
         type,
         menu->navigation.selection_ptr);

   return 0;
}

static int archive_load(void)
{
   int ret;
   const char *menu_path  = NULL;
   const char *menu_label = NULL;
   const char* path       = NULL;
   unsigned int type = 0;
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return -1;

   menu_list_pop_stack(menu->menu_list);

   menu_list_get_last_stack(menu->menu_list,
         &menu_path, &menu_label, NULL);

   if (menu_list_get_size(menu->menu_list) == 0)
      return 0;

   menu_list_get_at_offset(menu->menu_list->selection_buf,
         menu->navigation.selection_ptr, &path, NULL, &type);

   ret = rarch_defer_core(g_extern.core_info, menu_path, path, menu_label,
         menu->deferred_path, sizeof(menu->deferred_path));

   switch (ret)
   {
      case -1:
         rarch_main_command(RARCH_CMD_LOAD_CORE);
         menu_entries_common_load_content(false);
         break;
      case 0:
         menu_list_push_stack_refresh(
               menu->menu_list,
               g_settings.libretro_directory,
               "deferred_core_list",
               0,
               menu->navigation.selection_ptr);
         break;
   }

   return 0;
}

static int load_or_open_zip_iterate(unsigned action)
{
   char msg[PATH_MAX_LENGTH];
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return -1;

   snprintf(msg, sizeof(msg), "Opening compressed file\n"
         " \n"

         " - OK to open as Folder\n"
         " - Cancel/Back to Load \n");

   if (driver.video_data && driver.menu_ctx
         && driver.menu_ctx->render_messagebox)
   {
      if (*msg && msg[0] != '\0')
         driver.menu_ctx->render_messagebox(msg);
   }

   switch (action)
   {
      case MENU_ACTION_OK:
         archive_open();
         break;
      case MENU_ACTION_CANCEL:
         archive_load();
         break;
   }

   return 0;
}

static inline struct video_shader *shader_manager_get_current_shader(const char *label, unsigned type)
{
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return NULL;

   if (!strcmp(label, "video_shader_preset_parameters"))
      return menu->shader;
   else if (!strcmp(label, "video_shader_parameters"))
      return video_shader_driver_get_current_shader();
   return NULL;
}

static int mouse_post_iterate(menu_file_list_cbs_t *cbs, const char *path,
      const char *label, unsigned type, unsigned action)
{
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return -1;

   if (!menu->mouse.enable)
      return 0;

   if (menu->mouse.left)
   {
      if (!menu->mouse.oldleft)
      {
         menu->mouse.oldleft = true;

         if (cbs && cbs->action_ok)
            return cbs->action_ok(path, label, type,
                  menu->navigation.selection_ptr);
      }
   }
   else
      menu->mouse.oldleft = false;

   if (menu->mouse.right)
   {
      if (!menu->mouse.oldright)
      {
         menu->mouse.oldright = true;
         menu_list_pop_stack(menu->menu_list);
      }
   }
   else
      menu->mouse.oldright = false;

   return 0;
}

static int action_iterate_help(const char *label, unsigned action)
{
   unsigned i;
   static const unsigned binds[] = {
      RETRO_DEVICE_ID_JOYPAD_UP,
      RETRO_DEVICE_ID_JOYPAD_DOWN,
      RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_B,
      RETRO_DEVICE_ID_JOYPAD_SELECT,
      RARCH_MENU_TOGGLE,
      RARCH_QUIT_KEY,
      RETRO_DEVICE_ID_JOYPAD_X,
   };
   char desc[ARRAY_SIZE(binds)][64];
   char msg[PATH_MAX_LENGTH];
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return 0;

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   for (i = 0; i < ARRAY_SIZE(binds); i++)
   {
      const struct retro_keybind *keybind = (const struct retro_keybind*)
         &g_settings.input.binds[0][binds[i]];
      const struct retro_keybind *auto_bind = (const struct retro_keybind*)
         input_get_auto_bind(0, binds[i]);

      input_get_bind_string(desc[i], keybind, auto_bind, sizeof(desc[i]));
   }

   snprintf(msg, sizeof(msg),
         "-- Welcome to RetroArch --\n"
         " \n" // strtok_r doesn't split empty strings.

         "Basic Menu controls:\n"
         "    Scroll (Up): %-20s\n"
         "  Scroll (Down): %-20s\n"
         "      Accept/OK: %-20s\n"
         "           Back: %-20s\n"
         "           Info: %-20s\n"
         "Enter/Exit Menu: %-20s\n"
         " Exit RetroArch: %-20s\n"
         "Toggle Keyboard: %-20s\n"
         " \n"

         "To run content:\n"
         "Load a libretro core (Core).\n"
         "Load a content file (Load Content).     \n"
         " \n"
         "See Path Settings to set directories for faster access to files.\n"
         " \n"

         "Press Accept/OK to continue.",
      desc[0], desc[1], desc[2], desc[3], desc[4], desc[5], desc[6], desc[7]);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render_messagebox)
      driver.menu_ctx->render_messagebox(msg);

   if (action == MENU_ACTION_OK)
      menu_list_pop(menu->menu_list->menu_stack, NULL);

   return 0;
}

static int action_iterate_info(const char *label, unsigned action)
{
   char msg[PATH_MAX_LENGTH];
   char needle[PATH_MAX_LENGTH];
   unsigned info_type = 0;
   rarch_setting_t *current_setting = NULL;
   file_list_t *list = NULL;
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return 0;

   list = (file_list_t*)menu->menu_list->selection_buf;

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   current_setting = (rarch_setting_t*)setting_data_find_setting(
         menu->list_settings,
         list->list[menu->navigation.selection_ptr].label);

   if (current_setting)
      strlcpy(needle, current_setting->name, sizeof(needle));
   else if ((current_setting = (rarch_setting_t*)setting_data_find_setting(
               menu->list_settings,
               list->list[menu->navigation.selection_ptr].label)))
   {
      if (current_setting)
         strlcpy(needle, current_setting->name, sizeof(needle));
   }
   else
   {
      const char *lbl = NULL;
      menu_list_get_at_offset(list,
            menu->navigation.selection_ptr, NULL, &lbl,
            &info_type);

      if (lbl)
         strlcpy(needle, lbl, sizeof(needle));
   }

   setting_data_get_description(needle, msg, sizeof(msg));

   if (driver.video_data && driver.menu_ctx &&
         driver.menu_ctx->render_messagebox)
   {
      if (*msg && msg[0] != '\0')
         driver.menu_ctx->render_messagebox(msg);
   }

   if (action == MENU_ACTION_OK)
      menu_list_pop(menu->menu_list->menu_stack, &menu->navigation.selection_ptr);

   return 0;
}

static int action_iterate_load_open_zip(const char *label, unsigned action)
{
   switch (g_settings.archive.mode)
   {
      case 0:
         return load_or_open_zip_iterate(action);
      case 1:
         return archive_load();
      case 2:
         return archive_open();
      default:
         break;
   }

   return 0;
}

static int action_iterate_menu_viewport(const char *label, unsigned action)
{
   int stride_x = 1, stride_y = 1;
   char msg[PATH_MAX_LENGTH];
   struct retro_game_geometry *geom = NULL;
   const char *base_msg = NULL;
   unsigned type = 0;
   video_viewport_t *custom = &g_extern.console.screen.viewports.custom_vp;
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return -1;

   menu_list_get_last_stack(menu->menu_list, NULL, NULL, &type);

   geom = (struct retro_game_geometry*)&g_extern.system.av_info.geometry;

   if (g_settings.video.scale_integer)
   {
      stride_x = geom->base_width;
      stride_y = geom->base_height;
   }

   switch (action)
   {
      case MENU_ACTION_UP:
         if (type == MENU_SETTINGS_CUSTOM_VIEWPORT)
         {
            custom->y -= stride_y;
            custom->height += stride_y;
         }
         else if (custom->height >= (unsigned)stride_y)
            custom->height -= stride_y;

         rarch_main_command(RARCH_CMD_VIDEO_APPLY_STATE_CHANGES);
         break;

      case MENU_ACTION_DOWN:
         if (type == MENU_SETTINGS_CUSTOM_VIEWPORT)
         {
            custom->y += stride_y;
            if (custom->height >= (unsigned)stride_y)
               custom->height -= stride_y;
         }
         else
            custom->height += stride_y;

         rarch_main_command(RARCH_CMD_VIDEO_APPLY_STATE_CHANGES);
         break;

      case MENU_ACTION_LEFT:
         if (type == MENU_SETTINGS_CUSTOM_VIEWPORT)
         {
            custom->x -= stride_x;
            custom->width += stride_x;
         }
         else if (custom->width >= (unsigned)stride_x)
            custom->width -= stride_x;

         rarch_main_command(RARCH_CMD_VIDEO_APPLY_STATE_CHANGES);
         break;

      case MENU_ACTION_RIGHT:
         if (type == MENU_SETTINGS_CUSTOM_VIEWPORT)
         {
            custom->x += stride_x;
            if (custom->width >= (unsigned)stride_x)
               custom->width -= stride_x;
         }
         else
            custom->width += stride_x;

         rarch_main_command(RARCH_CMD_VIDEO_APPLY_STATE_CHANGES);
         break;

      case MENU_ACTION_CANCEL:
         menu_list_pop_stack(menu->menu_list);

         if (!strcmp(label, "custom_viewport_2"))
         {
            menu_list_push_stack(menu->menu_list, "", "",
                  MENU_SETTINGS_CUSTOM_VIEWPORT,
                  menu->navigation.selection_ptr);
         }
         break;

      case MENU_ACTION_OK:
         menu_list_pop_stack(menu->menu_list);

         if (type == MENU_SETTINGS_CUSTOM_VIEWPORT
               && !g_settings.video.scale_integer)
         {
            menu_list_push_stack(menu->menu_list, "",
                  "custom_viewport_2", 0, menu->navigation.selection_ptr);
         }
         break;

      case MENU_ACTION_START:
         if (!g_settings.video.scale_integer)
         {
            video_viewport_t vp;

            if (driver.video_data && driver.video &&
                  driver.video->viewport_info)
               driver.video->viewport_info(driver.video_data, &vp);

            if (type == MENU_SETTINGS_CUSTOM_VIEWPORT)
            {
               custom->width += custom->x;
               custom->height += custom->y;
               custom->x = 0;
               custom->y = 0;
            }
            else
            {
               custom->width = vp.full_width - custom->x;
               custom->height = vp.full_height - custom->y;
            }

            rarch_main_command(RARCH_CMD_VIDEO_APPLY_STATE_CHANGES);
         }
         break;

      case MENU_ACTION_MESSAGE:
         menu->msg_force = true;
         break;

      default:
         break;
   }

   menu_list_get_last_stack(menu->menu_list, NULL, &label, &type);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   if (g_settings.video.scale_integer)
   {
      custom->x = 0;
      custom->y = 0;
      custom->width = ((custom->width + geom->base_width - 1) /
            geom->base_width) * geom->base_width;
      custom->height = ((custom->height + geom->base_height - 1) /
            geom->base_height) * geom->base_height;

      base_msg = "Set scale";
      snprintf(msg, sizeof(msg), "%s (%4ux%4u, %u x %u scale)",
            base_msg,
            custom->width, custom->height,
            custom->width / geom->base_width,
            custom->height / geom->base_height);
   }
   else
   {
      if (type == MENU_SETTINGS_CUSTOM_VIEWPORT)
         base_msg = "Set Upper-Left Corner";
      else if (!strcmp(label, "custom_viewport_2"))
         base_msg = "Set Bottom-Right Corner";

      snprintf(msg, sizeof(msg), "%s (%d, %d : %4ux%4u)",
            base_msg, custom->x, custom->y, custom->width, custom->height);
   }

   if (driver.video_data && driver.menu_ctx &&
         driver.menu_ctx->render_messagebox)
      driver.menu_ctx->render_messagebox(msg);

   if (!custom->width)
      custom->width = stride_x;
   if (!custom->height)
      custom->height = stride_y;

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   rarch_main_command(RARCH_CMD_VIDEO_APPLY_STATE_CHANGES);

   return 0;
}

static int action_iterate_custom_bind(const char *label, unsigned action)
{
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return -1;
   if (menu_input_bind_iterate())
      menu_list_pop_stack(menu->menu_list);
   return 0;
}

static int action_iterate_custom_bind_keyboard(const char *label, unsigned action)
{
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return -1;
   if (menu_input_bind_iterate_keyboard())
      menu_list_pop_stack(menu->menu_list);
   return 0;
}

static int action_iterate_message(const char *label, unsigned action)
{
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return -1;

   if (driver.video_data && driver.menu_ctx
         && driver.menu_ctx->render_messagebox)
      driver.menu_ctx->render_messagebox(menu->message_contents);

   if (action == MENU_ACTION_OK)
      menu_list_pop_stack(menu->menu_list);

   return 0;
}

static int mouse_iterate(unsigned action)
{
   const struct retro_keybind *binds[MAX_USERS];
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return -1;

   if (!menu->mouse.enable)
      return 0;

   menu->mouse.dx = driver.input->input_state(driver.input_data,
         binds, 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
   menu->mouse.dy = driver.input->input_state(driver.input_data,
         binds, 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);

   menu->mouse.x += menu->mouse.dx;
   menu->mouse.y += menu->mouse.dy;

   if (menu->mouse.x < 5)
      menu->mouse.x = 5;
   if (menu->mouse.y < 5)
      menu->mouse.y = 5;
   if (menu->mouse.x > menu->frame_buf.width - 5)
      menu->mouse.x = menu->frame_buf.width - 5;
   if (menu->mouse.y > menu->frame_buf.height - 5)
      menu->mouse.y = menu->frame_buf.height - 5;

   menu->mouse.left = driver.input->input_state(driver.input_data,
         binds, 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);

   menu->mouse.right = driver.input->input_state(driver.input_data,
         binds, 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);

   menu->mouse.wheelup = driver.input->input_state(driver.input_data,
         binds, 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELUP)
         || menu->mouse.y == 5;

   menu->mouse.wheeldown = driver.input->input_state(driver.input_data,
         binds, 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN)
         || menu->mouse.y == menu->frame_buf.height - 5;

   return 0;
}

static int action_iterate_main(const char *label, unsigned action)
{
   int ret = 0;
   unsigned type_offset = 0;
   const char *label_offset = NULL;
   const char *path_offset = NULL;
   menu_file_list_cbs_t *cbs = NULL;
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return 0;

   cbs = (menu_file_list_cbs_t*)
      menu_list_get_actiondata_at_offset(menu->menu_list->selection_buf,
            menu->navigation.selection_ptr);

   menu_list_get_at_offset(menu->menu_list->selection_buf,
         menu->navigation.selection_ptr, &path_offset, &label_offset, &type_offset);

   mouse_iterate(action);

   if (!strcmp(label, "help"))
      return action_iterate_help(label, action);
   else if (!strcmp(label, "info_screen"))
      return action_iterate_info(label, action);
   else if (!strcmp(label, "load_open_zip"))
      return action_iterate_load_open_zip(label, action);
   else if (!strcmp(label, "message"))
      return action_iterate_message(label, action);
   else if (
         !strcmp(label, "custom_viewport_1") ||
         !strcmp(label, "custom_viewport_2")
         )
      return action_iterate_menu_viewport(label, action);
   else if (
         !strcmp(label, "custom_bind") ||
         !strcmp(label, "custom_bind_all") ||
         !strcmp(label, "custom_bind_defaults")
         )
   {
      if (g_extern.menu.bind_mode_keyboard)
         return action_iterate_custom_bind_keyboard(label, action);
      else
         return action_iterate_custom_bind(label, action);
   }

   if (menu->need_refresh && !menu->nonblocking_refresh && action != MENU_ACTION_MESSAGE)
      action = MENU_ACTION_REFRESH;

   switch (action)
   {
      case MENU_ACTION_UP:
      case MENU_ACTION_DOWN:
         if (cbs && cbs->action_up_or_down)
            ret = cbs->action_up_or_down(type_offset, label_offset, action);
         break;
      case MENU_ACTION_SCROLL_UP:
         menu_navigation_descend_alphabet(&menu->navigation, &menu->navigation.selection_ptr);
         break;
      case MENU_ACTION_SCROLL_DOWN:
         menu_navigation_ascend_alphabet(&menu->navigation, &menu->navigation.selection_ptr);
         break;

      case MENU_ACTION_CANCEL:
         if (cbs && cbs->action_cancel)
            return cbs->action_cancel(path_offset, label_offset, type_offset, menu->navigation.selection_ptr);
         break;

      case MENU_ACTION_OK:
         if (cbs && cbs->action_ok)
            return cbs->action_ok(path_offset, label_offset, type_offset, menu->navigation.selection_ptr);
         break;
      case MENU_ACTION_START:
         if (cbs && cbs->action_start)
            return cbs->action_start(type_offset, label_offset, action);
         break;
      case MENU_ACTION_LEFT:
      case MENU_ACTION_RIGHT:
         if (cbs && cbs->action_toggle)
            ret = cbs->action_toggle(type_offset, label_offset, action);
         break;
      case MENU_ACTION_SELECT:
         if (cbs && cbs->action_select)
            ret = cbs->action_select(type_offset, label_offset, action);
         break;

      case MENU_ACTION_REFRESH:
         if (cbs && cbs->action_refresh)
            ret = cbs->action_refresh(menu->menu_list->selection_buf,
                  menu->menu_list->menu_stack);
         break;

      case MENU_ACTION_MESSAGE:
         menu->msg_force = true;
         break;

      case MENU_ACTION_SEARCH:
         menu_input_search_start();
         break;

      case MENU_ACTION_TEST:
#if 0
         menu->rdl = database_info_write_rdl_init("/home/twinaphex/roms");

         if (!menu->rdl)
            return -1;
#endif
         break;

      default:
         break;
   }

   if (ret)
      return ret;

   ret = mouse_post_iterate(cbs, path_offset, label_offset, type_offset, action);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->render)
      driver.menu_ctx->render();

   /* Have to defer it so we let settings refresh. */
   if (menu->push_start_screen)
   {
      menu_list_push_stack(menu->menu_list, "", "help", 0, 0);
      menu->push_start_screen = false;
   }

   return ret;
}

void menu_entries_cbs_init_bind_iterate(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   if (cbs)
      cbs->action_iterate = action_iterate_main;
}
