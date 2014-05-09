/*
 * Copyright (c) 2013 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"

#include "gtkheaderbar.h"
#include "gtkheaderbarprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkcontainerprivate.h"
#include "a11y/gtkcontaineraccessible.h"

#include <string.h>

/**
 * SECTION:gtkheaderbar
 * @Short_description: A box with a centered child
 * @Title: GtkHeaderBar
 * @See_also: #GtkBox
 *
 * GtkHeaderBar is similar to a horizontal #GtkBox, it allows
 * to place children at the start or the end. In addition,
 * it allows a title to be displayed. The title will be
 * centered with respect to the width of the box, even if the children
 * at either side take up different amounts of space.
 */

#define DEFAULT_SPACING 6

struct _GtkHeaderBarPrivate
{
  gchar *title;
  gchar *subtitle;
  GtkWidget *title_label;
  GtkWidget *subtitle_label;
  GtkWidget *label_box;
  GtkWidget *label_sizing_box;
  GtkWidget *custom_title;
  GtkWidget *close_button;
  GtkWidget *separator;
  gint spacing;

  GList *children;
};

typedef struct _Child Child;
struct _Child
{
  GtkWidget *widget;
  GtkPackType pack_type;
};

enum {
  PROP_0,
  PROP_TITLE,
  PROP_SUBTITLE,
  PROP_CUSTOM_TITLE,
  PROP_SPACING,
  PROP_SHOW_CLOSE_BUTTON
};

enum {
  CHILD_PROP_0,
  CHILD_PROP_PACK_TYPE,
  CHILD_PROP_POSITION
};

static void gtk_header_bar_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkHeaderBar, gtk_header_bar, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkHeaderBar)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_header_bar_buildable_init));

static void
get_css_padding_and_border (GtkWidget *widget,
                            GtkBorder *border)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder tmp;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_padding (context, state, border);
  gtk_style_context_get_border (context, state, &tmp);
  border->top += tmp.top;
  border->right += tmp.right;
  border->bottom += tmp.bottom;
  border->left += tmp.left;
}

static void
init_sizing_box (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GtkWidget *w;
  GtkStyleContext *context;

  /* We use this box to always request size for the two labels (title
   * and subtitle) as if they were always visible, but then allocate
   * the real label box with its actual size, to keep it center-aligned
   * in case we have only the title.
   */
  priv->label_sizing_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  w = gtk_label_new (NULL);
  context = gtk_widget_get_style_context (w);
  gtk_style_context_add_class (context, "title");
  gtk_box_pack_start (GTK_BOX (priv->label_sizing_box), w, FALSE, FALSE, 0);
  gtk_label_set_line_wrap (GTK_LABEL (w), FALSE);
  gtk_label_set_single_line_mode (GTK_LABEL (w), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (w), PANGO_ELLIPSIZE_END);

  w = gtk_label_new (NULL);
  context = gtk_widget_get_style_context (w);
  gtk_style_context_add_class (context, "subtitle");
  gtk_style_context_add_class (context, "dim-label");
  gtk_box_pack_start (GTK_BOX (priv->label_sizing_box), w, FALSE, FALSE, 0);
  gtk_label_set_line_wrap (GTK_LABEL (w), FALSE);
  gtk_label_set_single_line_mode (GTK_LABEL (w), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (w), PANGO_ELLIPSIZE_END);

  gtk_widget_show_all (priv->label_sizing_box);
}

GtkWidget *
_gtk_header_bar_create_title_box (const char *title,
                                  const char *subtitle,
                                  GtkWidget **ret_title_label,
                                  GtkWidget **ret_subtitle_label)
{
  GtkWidget *label_box;
  GtkWidget *title_label;
  GtkWidget *subtitle_label;
  GtkStyleContext *context;

  label_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_valign (label_box, GTK_ALIGN_CENTER);
  gtk_widget_show (label_box);

  title_label = gtk_label_new (title);
  context = gtk_widget_get_style_context (title_label);
  gtk_style_context_add_class (context, "title");
  gtk_label_set_line_wrap (GTK_LABEL (title_label), FALSE);
  gtk_label_set_single_line_mode (GTK_LABEL (title_label), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (title_label), PANGO_ELLIPSIZE_END);
  gtk_box_pack_start (GTK_BOX (label_box), title_label, FALSE, FALSE, 0);
  gtk_widget_show (title_label);

  subtitle_label = gtk_label_new (subtitle);
  context = gtk_widget_get_style_context (subtitle_label);
  gtk_style_context_add_class (context, "subtitle");
  gtk_style_context_add_class (context, "dim-label");
  gtk_label_set_line_wrap (GTK_LABEL (subtitle_label), FALSE);
  gtk_label_set_single_line_mode (GTK_LABEL (subtitle_label), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (subtitle_label), PANGO_ELLIPSIZE_END);
  gtk_box_pack_start (GTK_BOX (label_box), subtitle_label, FALSE, FALSE, 0);
  gtk_widget_set_no_show_all (subtitle_label, TRUE);

  if (ret_title_label)
    *ret_title_label = title_label;
  if (ret_subtitle_label)
    *ret_subtitle_label = subtitle_label;

  return label_box;
}

static void
close_button_clicked (GtkButton *button, gpointer data)
{
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
  gtk_window_close (GTK_WINDOW (toplevel));
}

static void
add_close_button (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv;
  GtkWidget *button;
  GIcon *icon;
  GtkWidget *image;
  GtkWidget *separator;
  GtkStyleContext *context;
  AtkObject *accessible;

  priv = gtk_header_bar_get_instance_private (bar);

  button = gtk_button_new ();
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "image-button");
  gtk_style_context_add_class (context, "titlebutton");
  icon = g_themed_icon_new ("window-close-symbolic");
  image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
  g_object_unref (icon);
  gtk_container_add (GTK_CONTAINER (button), image);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (close_button_clicked), NULL);
  accessible = gtk_widget_get_accessible (button);
  if (GTK_IS_ACCESSIBLE (accessible))
    atk_object_set_name (accessible, _("Close"));
  gtk_widget_show_all (button);
  gtk_widget_set_parent (button, GTK_WIDGET (bar));

  separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
  gtk_widget_show (separator);
  gtk_widget_set_parent (separator, GTK_WIDGET (bar));

  priv->separator = separator;
  priv->close_button = button;
}

static void
remove_close_button (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  gtk_widget_unparent (priv->separator);
  gtk_widget_unparent (priv->close_button);

  priv->separator = NULL;
  priv->close_button = NULL;
}

static void
construct_label_box (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  g_assert (priv->label_box == NULL);

  priv->label_box = _gtk_header_bar_create_title_box (priv->title,
                                                      priv->subtitle,
                                                      &priv->title_label,
                                                      &priv->subtitle_label);
  gtk_widget_set_parent (priv->label_box, GTK_WIDGET (bar));
}

static void
gtk_header_bar_init (GtkHeaderBar *bar)
{
  GtkStyleContext *context;
  GtkHeaderBarPrivate *priv;

  priv = gtk_header_bar_get_instance_private (bar);

  gtk_widget_set_has_window (GTK_WIDGET (bar), FALSE);
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (bar), FALSE);

  priv->title = NULL;
  priv->subtitle = NULL;
  priv->custom_title = NULL;
  priv->close_button = NULL;
  priv->separator = NULL;
  priv->children = NULL;
  priv->spacing = DEFAULT_SPACING;

  init_sizing_box (bar);
  construct_label_box (bar);

  context = gtk_widget_get_style_context (GTK_WIDGET (bar));
  gtk_style_context_add_class (context, "header-bar");
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_HORIZONTAL);
}

static gint
count_visible_children (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GList *l;
  Child *child;
  gint n;

  n = 0;
  for (l = priv->children; l; l = l->next)
    {
      child = l->data;
      if (gtk_widget_get_visible (child->widget))
        n++;
    }

  return n;
}

static gboolean
add_child_size (GtkWidget      *child,
                GtkOrientation  orientation,
                gint           *minimum,
                gint           *natural)
{
  gint child_minimum, child_natural;

  if (!gtk_widget_get_visible (child))
    return FALSE;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_widget_get_preferred_width (child, &child_minimum, &child_natural);
  else
    gtk_widget_get_preferred_height (child, &child_minimum, &child_natural);

  if (GTK_ORIENTATION_HORIZONTAL == orientation)
    {
      *minimum += child_minimum;
      *natural += child_natural;
    }
  else
    {
      *minimum = MAX (*minimum, child_minimum);
      *natural = MAX (*natural, child_natural);
    }

  return TRUE;
}

static void
gtk_header_bar_get_size (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         gint           *minimum_size,
                         gint           *natural_size)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (widget);
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GList *l;
  gint nvis_children;
  gint minimum, natural;
  GtkBorder css_borders;

  minimum = natural = 0;
  nvis_children = 0;

  for (l = priv->children; l; l = l->next)
    {
      Child *child = l->data;

      if (add_child_size (child->widget, orientation, &minimum, &natural))
        nvis_children += 1;
    }

  if (priv->label_box != NULL)
    {
      if (add_child_size (priv->label_sizing_box, orientation, &minimum, &natural))
        nvis_children += 1;
    }

  if (priv->custom_title != NULL)
    {
      if (add_child_size (priv->custom_title, orientation, &minimum, &natural))
        nvis_children += 1;
    }

  if (priv->close_button != NULL)
    {
      if (add_child_size (priv->close_button, orientation, &minimum, &natural))
        nvis_children += 1;

      if (add_child_size (priv->separator, orientation, &minimum, &natural))
        nvis_children += 1;
    }

  if (nvis_children > 0 && orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      minimum += nvis_children * priv->spacing;
      natural += nvis_children * priv->spacing;
    }

  get_css_padding_and_border (widget, &css_borders);

  if (GTK_ORIENTATION_HORIZONTAL == orientation)
    {
      minimum += css_borders.left + css_borders.right;
      natural += css_borders.left + css_borders.right;
    }
  else
    {
      minimum += css_borders.top + css_borders.bottom;
      natural += css_borders.top + css_borders.bottom;
    }

  if (minimum_size)
    *minimum_size = minimum;

  if (natural_size)
    *natural_size = natural;
}

static void
gtk_header_bar_compute_size_for_orientation (GtkWidget *widget,
                                             gint       avail_size,
                                             gint      *minimum_size,
                                             gint      *natural_size)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (widget);
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GList *children;
  gint required_size = 0;
  gint required_natural = 0;
  gint child_size;
  gint child_natural;
  gint nvis_children;
  GtkBorder css_borders;

  nvis_children = 0;

  for (children = priv->children; children != NULL; children = children->next)
    {
      Child *child = children->data;

      if (gtk_widget_get_visible (child->widget))
        {
          gtk_widget_get_preferred_width_for_height (child->widget,
                                                     avail_size, &child_size, &child_natural);

          required_size += child_size;
          required_natural += child_natural;

          nvis_children += 1;
        }
    }

  if (priv->label_box != NULL &&
      gtk_widget_get_visible (priv->label_box))
    {
      gtk_widget_get_preferred_width (priv->label_sizing_box,
                                      &child_size, &child_natural);
      required_size += child_size;
      required_natural += child_natural;
    }

  if (priv->custom_title != NULL &&
      gtk_widget_get_visible (priv->custom_title))
    {
      gtk_widget_get_preferred_width (priv->custom_title,
                                      &child_size, &child_natural);
      required_size += child_size;
      required_natural += child_natural;
    }

  if (priv->close_button != NULL)
    {
      gtk_widget_get_preferred_width (priv->close_button,
                                      &child_size, &child_natural);
      required_size += child_size;
      required_natural += child_natural;

      gtk_widget_get_preferred_width (priv->separator,
                                      &child_size, &child_natural);
      required_size += child_size;
      required_natural += child_natural;
    }

  if (nvis_children > 0)
    {
      required_size += nvis_children * priv->spacing;
      required_natural += nvis_children * priv->spacing;
    }

  get_css_padding_and_border (widget, &css_borders);

  required_size += css_borders.left + css_borders.right;
  required_natural += css_borders.left + css_borders.right;

  if (minimum_size)
    *minimum_size = required_size;

  if (natural_size)
    *natural_size = required_natural;
}

static void
gtk_header_bar_compute_size_for_opposing_orientation (GtkWidget *widget,
                                                      gint       avail_size,
                                                      gint      *minimum_size,
                                                      gint      *natural_size)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (widget);
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  Child *child;
  GList *children;
  gint nvis_children;
  gint computed_minimum = 0;
  gint computed_natural = 0;
  GtkRequestedSize *sizes;
  GtkPackType packing;
  gint size = 0;
  gint i;
  gint child_size;
  gint child_minimum;
  gint child_natural;
  GtkBorder css_borders;

  nvis_children = count_visible_children (bar);

  if (nvis_children <= 0)
    return;

  sizes = g_newa (GtkRequestedSize, nvis_children);

  /* Retrieve desired size for visible children */
  for (i = 0, children = priv->children; children; children = children->next)
    {
      child = children->data;

      if (gtk_widget_get_visible (child->widget))
        {
          gtk_widget_get_preferred_width (child->widget,
                                          &sizes[i].minimum_size,
                                          &sizes[i].natural_size);

          size -= sizes[i].minimum_size;
          sizes[i].data = child;
          i += 1;
        }
    }

  /* Bring children up to size first */
  size = gtk_distribute_natural_allocation (MAX (0, avail_size), nvis_children, sizes);

  /* Allocate child positions. */
  for (packing = GTK_PACK_START; packing <= GTK_PACK_END; ++packing)
    {
      for (i = 0, children = priv->children; children; children = children->next)
        {
          child = children->data;

          /* If widget is not visible, skip it. */
          if (!gtk_widget_get_visible (child->widget))
            continue;

          /* If widget is packed differently skip it, but still increment i,
           * since widget is visible and will be handled in next loop
           * iteration.
           */
          if (child->pack_type != packing)
            {
              i++;
              continue;
            }

          child_size = sizes[i].minimum_size;

          gtk_widget_get_preferred_height_for_width (child->widget,
                                                     child_size, &child_minimum, &child_natural);

          computed_minimum = MAX (computed_minimum, child_minimum);
          computed_natural = MAX (computed_natural, child_natural);
        }
      i += 1;
    }

  if (priv->label_box != NULL &&
      gtk_widget_get_visible (priv->label_box))
    {
      gtk_widget_get_preferred_height (priv->label_sizing_box,
                                       &child_minimum, &child_natural);
      computed_minimum = MAX (computed_minimum, child_minimum);
      computed_natural = MAX (computed_natural, child_natural);
    }

  if (priv->custom_title != NULL &&
      gtk_widget_get_visible (priv->custom_title))
    {
      gtk_widget_get_preferred_height (priv->custom_title,
                                       &child_minimum, &child_natural);
      computed_minimum = MAX (computed_minimum, child_minimum);
      computed_natural = MAX (computed_natural, child_natural);
    }

  if (priv->close_button != NULL)
    {
      gtk_widget_get_preferred_height (priv->close_button,
                                       &child_minimum, &child_natural);
      computed_minimum = MAX (computed_minimum, child_minimum);
      computed_natural = MAX (computed_natural, child_natural);

      gtk_widget_get_preferred_height (priv->separator,
                                       &child_minimum, &child_natural);
      computed_minimum = MAX (computed_minimum, child_minimum);
      computed_natural = MAX (computed_natural, child_natural);
    }

  get_css_padding_and_border (widget, &css_borders);

  computed_minimum += css_borders.top + css_borders.bottom;
  computed_natural += css_borders.top + css_borders.bottom;

  if (minimum_size)
    *minimum_size = computed_minimum;

  if (natural_size)
    *natural_size = computed_natural;
}

static void
gtk_header_bar_get_preferred_width (GtkWidget *widget,
                                    gint      *minimum_size,
                                    gint      *natural_size)
{
  gtk_header_bar_get_size (widget, GTK_ORIENTATION_HORIZONTAL, minimum_size, natural_size);
}

static void
gtk_header_bar_get_preferred_height (GtkWidget *widget,
                                     gint      *minimum_size,
                                     gint      *natural_size)
{
  gtk_header_bar_get_size (widget, GTK_ORIENTATION_VERTICAL, minimum_size, natural_size);
}

static void
gtk_header_bar_get_preferred_width_for_height (GtkWidget *widget,
                                               gint       height,
                                               gint      *minimum_width,
                                               gint      *natural_width)
{
  gtk_header_bar_compute_size_for_orientation (widget, height, minimum_width, natural_width);
}

static void
gtk_header_bar_get_preferred_height_for_width (GtkWidget *widget,
                                               gint       width,
                                               gint      *minimum_height,
                                               gint      *natural_height)
{
  gtk_header_bar_compute_size_for_opposing_orientation (widget, width, minimum_height, natural_height);
}

static gboolean
close_button_at_end (GtkWidget *widget)
{
  GtkWidget *toplevel;
  gchar *layout_desc;
  gboolean at_end;
  gchar *p;

  toplevel = gtk_widget_get_toplevel (widget);
  if (!GTK_IS_WINDOW (toplevel))
    return TRUE;
  gtk_widget_style_get (toplevel, "decoration-button-layout", &layout_desc, NULL);

  p = strchr (layout_desc, ':');
  if (p && strstr (p, "close"))
    at_end = TRUE;
  else
    at_end = FALSE;

  g_free (layout_desc);

  return at_end;
}

static void
gtk_header_bar_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (widget);
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GtkRequestedSize *sizes;
  gint width, height;
  gint nvis_children;
  gint title_minimum_size;
  gint title_natural_size;
  gint close_button_width;
  gint separator_width;
  gint close_width;
  gint side[2];
  GList *l;
  gint i;
  Child *child;
  GtkPackType packing;
  GtkAllocation child_allocation;
  gint x;
  gint child_size;
  GtkTextDirection direction;
  GtkBorder css_borders;
  gboolean at_end;

  at_end = close_button_at_end (widget);

  gtk_widget_set_allocation (widget, allocation);

  direction = gtk_widget_get_direction (widget);
  nvis_children = count_visible_children (bar);
  sizes = g_newa (GtkRequestedSize, nvis_children);

  get_css_padding_and_border (widget, &css_borders);
  width = allocation->width - nvis_children * priv->spacing - css_borders.left - css_borders.right;
  height = allocation->height - css_borders.top - css_borders.bottom;

  i = 0;
  for (l = priv->children; l; l = l->next)
    {
      child = l->data;
      if (!gtk_widget_get_visible (child->widget))
        continue;

      gtk_widget_get_preferred_width_for_height (child->widget,
                                                 height,
                                                 &sizes[i].minimum_size,
                                                 &sizes[i].natural_size);
      width -= sizes[i].minimum_size;
      i++;
    }

  if (priv->custom_title)
    {
      gtk_widget_get_preferred_width_for_height (priv->custom_title,
                                                 height,
                                                 &title_minimum_size,
                                                 &title_natural_size);
    }
  else
    {
      gtk_widget_get_preferred_width_for_height (priv->label_box,
                                                 height,
                                                 &title_minimum_size,
                                                 &title_natural_size);
    }
  width -= title_natural_size;

  close_button_width = separator_width = close_width = 0;
  if (priv->close_button != NULL)
    {
      gint min, nat;
      gtk_widget_get_preferred_width_for_height (priv->close_button,
                                                 height,
                                                 &min, &nat);
      close_button_width = nat;

      gtk_widget_get_preferred_width_for_height (priv->separator,
                                                 height,
                                                 &min, &nat);
      separator_width = nat;
      close_width = close_button_width + separator_width + 2 * priv->spacing;
    }
  width -= close_width;

  width = gtk_distribute_natural_allocation (MAX (0, width), nvis_children, sizes);

  side[0] = side[1] = 0;
  for (packing = GTK_PACK_START; packing <= GTK_PACK_END; packing++)
    {
      child_allocation.y = allocation->y + css_borders.top;
      child_allocation.height = height;
      if (packing == GTK_PACK_START)
        x = allocation->x + css_borders.left + (at_end ? 0 : close_width);
      else
        x = allocation->x + allocation->width - (at_end ? close_width : 0) - css_borders.right;

      if (packing == GTK_PACK_START)
	{
	  l = priv->children;
	  i = 0;
	}
      else
	{
	  l = g_list_last (priv->children);
	  i = nvis_children - 1;
	}

      for (; l != NULL; (packing == GTK_PACK_START) ? (l = l->next) : (l = l->prev))
        {
          child = l->data;
          if (!gtk_widget_get_visible (child->widget))
            continue;

          if (child->pack_type != packing)
            goto next;

          child_size = sizes[i].minimum_size;

          child_allocation.width = child_size;

          if (packing == GTK_PACK_START)
            {
              child_allocation.x = x;
              x += child_size;
              x += priv->spacing;
            }
          else
            {
              x -= child_size;
              child_allocation.x = x;
              x -= priv->spacing;
            }

          side[packing] += child_size + priv->spacing;

          if (direction == GTK_TEXT_DIR_RTL)
            child_allocation.x = allocation->x + allocation->width - (child_allocation.x - allocation->x) - child_allocation.width;

          gtk_widget_size_allocate (child->widget, &child_allocation);

        next:
          if (packing == GTK_PACK_START)
            i++;
          else
            i--;
        }
    }

  if (at_end)
    side[GTK_PACK_END] += close_width;
  else
    side[GTK_PACK_START] += close_width;

  child_allocation.y = allocation->y + css_borders.top;
  child_allocation.height = height;

  width = MAX (side[0], side[1]);

  if (allocation->width - 2 * width >= title_natural_size)
    child_size = MIN (title_natural_size, allocation->width - 2 * width);
  else if (allocation->width - side[0] - side[1] >= title_natural_size)
    child_size = MIN (title_natural_size, allocation->width - side[0] - side[1]);
  else
    child_size = allocation->width - side[0] - side[1];

  child_allocation.x = allocation->x + (allocation->width - child_size) / 2;
  child_allocation.width = child_size;

  if (allocation->x + side[0] > child_allocation.x)
    child_allocation.x = allocation->x + side[0];
  else if (allocation->x + allocation->width - side[1] < child_allocation.x + child_allocation.width)
    child_allocation.x = allocation->x + allocation->width - side[1] - child_allocation.width;

  if (direction == GTK_TEXT_DIR_RTL)
    child_allocation.x = allocation->x + allocation->width - (child_allocation.x - allocation->x) - child_allocation.width;

  if (priv->custom_title)
    gtk_widget_size_allocate (priv->custom_title, &child_allocation);
  else
    gtk_widget_size_allocate (priv->label_box, &child_allocation);

  if (priv->close_button)
    {
      gboolean left;

      if (direction == GTK_TEXT_DIR_RTL)
        left = at_end;
      else
        left = !at_end;

      if (left)
        child_allocation.x = allocation->x + css_borders.left;
      else
        child_allocation.x = allocation->x + allocation->width - css_borders.right - close_button_width;
      child_allocation.width = close_button_width;
      gtk_widget_size_allocate (priv->close_button, &child_allocation);

      if (left)
        child_allocation.x = allocation->x + css_borders.left + close_button_width + priv->spacing;
      else
        child_allocation.x = allocation->x + allocation->width - css_borders.right - close_button_width - priv->spacing - separator_width;
      child_allocation.width = separator_width;
      gtk_widget_size_allocate (priv->separator, &child_allocation);
    }
}

/**
 * gtk_header_bar_set_title:
 * @bar: a #GtkHeaderBar
 * @title: (allow-none): a title
 *
 * Sets the title of the #GtkHeaderBar. The title should help a user
 * identify the current view. A good title should not include the
 * application name.
 *
 * Since: 3.10
 */
void
gtk_header_bar_set_title (GtkHeaderBar *bar,
                          const gchar  *title)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  gchar *new_title;

  g_return_if_fail (GTK_IS_HEADER_BAR (bar));

  new_title = g_strdup (title);
  g_free (priv->title);
  priv->title = new_title;

  if (priv->title_label != NULL)
    {
      gtk_label_set_label (GTK_LABEL (priv->title_label), priv->title);
      gtk_widget_queue_resize (GTK_WIDGET (bar));
    }

  g_object_notify (G_OBJECT (bar), "title");
}

/**
 * gtk_header_bar_get_title:
 * @bar: a #GtkHeaderBar
 *
 * Retrieves the title of the header. See gtk_header_bar_set_title().
 *
 * Return value: the title of the header, or %NULL if none has
 *    been set explicitely. The returned string is owned by the widget
 *    and must not be modified or freed.
 *
 * Since: 3.10
 */
const gchar *
gtk_header_bar_get_title (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  g_return_val_if_fail (GTK_IS_HEADER_BAR (bar), NULL);

  return priv->title;
}

/**
 * gtk_header_bar_set_subtitle:
 * @bar: a #GtkHeaderBar
 * @subtitle: (allow-none): a subtitle
 *
 * Sets the subtitle of the #GtkHeaderBar. The title should give a user
 * an additional detail to help him identify the current view.
 *
 * Note that GtkHeaderBar always reserves room for the subtitle, even
 * if none is currently set. If this is not desired, use
 * gtk_header_bar_set_custom_title() to place your own label in the
 * title position. To achieve the same style, use the "title" style
 * class.
 *
 * Since: 3.10
 */
void
gtk_header_bar_set_subtitle (GtkHeaderBar *bar,
                             const gchar  *subtitle)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  gchar *new_subtitle;

  g_return_if_fail (GTK_IS_HEADER_BAR (bar));

  new_subtitle = g_strdup (subtitle);
  g_free (priv->subtitle);
  priv->subtitle = new_subtitle;

  if (priv->subtitle_label != NULL)
    {
      gtk_label_set_label (GTK_LABEL (priv->subtitle_label), priv->subtitle);
      gtk_widget_set_visible (priv->subtitle_label, priv->subtitle != NULL);
      gtk_widget_queue_resize (GTK_WIDGET (bar));
    }

  g_object_notify (G_OBJECT (bar), "subtitle");
}

/**
 * gtk_header_bar_get_subtitle:
 * @bar: a #GtkHeaderBar
 *
 * Retrieves the subtitle of the header. See gtk_header_bar_set_subtitle().
 *
 * Return value: the subtitle of the header, or %NULL if none has
 *    been set explicitely. The returned string is owned by the widget
 *    and must not be modified or freed.
 *
 * Since: 3.10
 */
const gchar *
gtk_header_bar_get_subtitle (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  g_return_val_if_fail (GTK_IS_HEADER_BAR (bar), NULL);

  return priv->subtitle;
}

/**
 * gtk_header_bar_set_custom_title:
 * @bar: a #GtkHeaderBar
 * @title_widget: (allow-none): a custom widget to use for a title
 *
 * Sets a custom title for the #GtkHeaderBar. The title should help a
 * user identify the current view. This supercedes any title set by
 * gtk_header_bar_set_title() or gtk_header_bar_set_subtitle().
 * You should set the custom title to %NULL, for the header title label
 * to be visible again.
 *
 * Since: 3.10
 */
void
gtk_header_bar_set_custom_title (GtkHeaderBar *bar,
                                 GtkWidget    *title_widget)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  g_return_if_fail (GTK_IS_HEADER_BAR (bar));
  if (title_widget)
    g_return_if_fail (GTK_IS_WIDGET (title_widget));

  /* No need to do anything if the custom widget stays the same */
  if (priv->custom_title == title_widget)
    return;

  if (priv->custom_title)
    {
      GtkWidget *custom = priv->custom_title;

      priv->custom_title = NULL;
      gtk_widget_unparent (custom);
    }

  if (title_widget != NULL)
    {
      priv->custom_title = title_widget;

      gtk_widget_set_parent (priv->custom_title, GTK_WIDGET (bar));
      gtk_widget_set_valign (priv->custom_title, GTK_ALIGN_CENTER);
      gtk_widget_show (title_widget);

      if (priv->label_box != NULL)
        {
          GtkWidget *label_box = priv->label_box;

          priv->label_box = NULL;
          priv->title_label = NULL;
          priv->subtitle_label = NULL;
          gtk_widget_unparent (label_box);
        }

    }
  else
    {
      if (priv->label_box == NULL)
        construct_label_box (bar);
    }

  gtk_widget_queue_resize (GTK_WIDGET (bar));

  g_object_notify (G_OBJECT (bar), "custom-title");
}

/**
 * gtk_header_bar_get_custom_title:
 * @bar: a #GtkHeaderBar
 *
 * Retrieves the custom title widget of the header. See
 * gtk_header_bar_set_custom_title().
 *
 * Return value: (transfer none): the custom title widget
 *    of the header, or %NULL if none has been set explicitely.
 *
 * Since: 3.10
 */
GtkWidget *
gtk_header_bar_get_custom_title (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  g_return_val_if_fail (GTK_IS_HEADER_BAR (bar), NULL);

  return priv->custom_title;
}

static void
gtk_header_bar_finalize (GObject *object)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (GTK_HEADER_BAR (object));

  g_free (priv->title);
  g_free (priv->subtitle);

  G_OBJECT_CLASS (gtk_header_bar_parent_class)->finalize (object);
}

static void
gtk_header_bar_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (object);
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, priv->subtitle);
      break;

    case PROP_CUSTOM_TITLE:
      g_value_set_object (value, priv->custom_title);
      break;

    case PROP_SPACING:
      g_value_set_int (value, priv->spacing);
      break;

    case PROP_SHOW_CLOSE_BUTTON:
      g_value_set_boolean (value, gtk_header_bar_get_show_close_button (bar));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_header_bar_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (object);
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  switch (prop_id)
    {
    case PROP_TITLE:
      gtk_header_bar_set_title (bar, g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      gtk_header_bar_set_subtitle (bar, g_value_get_string (value));
      break;

    case PROP_CUSTOM_TITLE:
      gtk_header_bar_set_custom_title (bar, g_value_get_object (value));
      break;

    case PROP_SPACING:
      priv->spacing = g_value_get_int (value);
      gtk_widget_queue_resize (GTK_WIDGET (bar));
      break;

    case PROP_SHOW_CLOSE_BUTTON:
      gtk_header_bar_set_show_close_button (bar, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_header_bar_pack (GtkHeaderBar *bar,
                     GtkWidget    *widget,
                     GtkPackType   pack_type)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  Child *child;

  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);

  child = g_new (Child, 1);
  child->widget = widget;
  child->pack_type = pack_type;

  priv->children = g_list_append (priv->children, child);

  gtk_widget_freeze_child_notify (widget);
  gtk_widget_set_parent (widget, GTK_WIDGET (bar));
  gtk_widget_child_notify (widget, "pack-type");
  gtk_widget_child_notify (widget, "position");
  gtk_widget_thaw_child_notify (widget);
}

static void
gtk_header_bar_add (GtkContainer *container,
                    GtkWidget    *child)
{
  gtk_header_bar_pack (GTK_HEADER_BAR (container), child, GTK_PACK_START);
}

static GList *
find_child_link (GtkHeaderBar *bar,
                 GtkWidget    *widget)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GList *l;
  Child *child;

  for (l = priv->children; l; l = l->next)
    {
      child = l->data;
      if (child->widget == widget)
        return l;
    }

  return NULL;
}

static void
gtk_header_bar_remove (GtkContainer *container,
                       GtkWidget    *widget)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (container);
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GList *l;
  Child *child;

  l = find_child_link (bar, widget);
  if (l)
    {
      child = l->data;
      gtk_widget_unparent (child->widget);
      priv->children = g_list_delete_link (priv->children, l);
      g_free (child);
      gtk_widget_queue_resize (GTK_WIDGET (container));
    }
}

static void
gtk_header_bar_forall (GtkContainer *container,
                       gboolean      include_internals,
                       GtkCallback   callback,
                       gpointer      callback_data)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (container);
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  Child *child;
  GList *children;

  children = priv->children;
  while (children)
    {
      child = children->data;
      children = children->next;
      if (child->pack_type == GTK_PACK_START)
        (* callback) (child->widget, callback_data);
    }

  if (priv->custom_title != NULL)
    (* callback) (priv->custom_title, callback_data);

  if (include_internals && priv->label_box != NULL)
    (* callback) (priv->label_box, callback_data);

  if (include_internals && priv->close_button != NULL)
    (* callback) (priv->close_button, callback_data);

  if (include_internals && priv->separator != NULL)
    (* callback) (priv->separator, callback_data);

  children = priv->children;
  while (children)
    {
      child = children->data;
      children = children->next;
      if (child->pack_type == GTK_PACK_END)
        (* callback) (child->widget, callback_data);
    }
}

static GType
gtk_header_bar_child_type (GtkContainer *container)
{
  return GTK_TYPE_WIDGET;
}

static void
gtk_header_bar_get_child_property (GtkContainer *container,
                                   GtkWidget    *widget,
                                   guint         property_id,
                                   GValue       *value,
                                   GParamSpec   *pspec)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (container);
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GList *l;
  Child *child;

  l = find_child_link (bar, widget);
  if (l == NULL)
    {
      g_param_value_set_default (pspec, value);
      return;
    }

  child = l->data;

  switch (property_id)
    {
    case CHILD_PROP_PACK_TYPE:
      g_value_set_enum (value, child->pack_type);
      break;

    case CHILD_PROP_POSITION:
      g_value_set_int (value, g_list_position (priv->children, l));
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_header_bar_set_child_property (GtkContainer *container,
                                   GtkWidget    *widget,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GList *l;
  Child *child;

  l = find_child_link (GTK_HEADER_BAR (container), widget);
  child = l->data;

  switch (property_id)
    {
    case CHILD_PROP_PACK_TYPE:
      child->pack_type = g_value_get_enum (value);
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static GtkWidgetPath *
gtk_header_bar_get_path_for_child (GtkContainer *container,
                                   GtkWidget    *child)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (container);
  GtkWidgetPath *path, *sibling_path;
  GList *list, *children;

  path = _gtk_widget_create_path (GTK_WIDGET (container));

  if (gtk_widget_get_visible (child))
    {
      gint i, position;

      sibling_path = gtk_widget_path_new ();

      /* get_all_children works in reverse (!) visible order */
      children = _gtk_container_get_all_children (container);
      if (gtk_widget_get_direction (GTK_WIDGET (bar)) == GTK_TEXT_DIR_LTR)
        children = g_list_reverse (children);

      position = -1;
      i = 0;
      for (list = children; list; list = list->next)
        {
          if (!gtk_widget_get_visible (list->data))
            continue;

          gtk_widget_path_append_for_widget (sibling_path, list->data);

          if (list->data == child)
            position = i;
          i++;
        }
      g_list_free (children);

      if (position >= 0)
        gtk_widget_path_append_with_siblings (path, sibling_path, position);
      else
        gtk_widget_path_append_for_widget (path, child);

      gtk_widget_path_unref (sibling_path);
    }
  else
    gtk_widget_path_append_for_widget (path, child);

  return path;
}

static gint
gtk_header_bar_draw (GtkWidget *widget,
                     cairo_t   *cr)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  gtk_render_background (context, cr, 0, 0,
                         gtk_widget_get_allocated_width (widget),
                         gtk_widget_get_allocated_height (widget));
  gtk_render_frame (context, cr, 0, 0,
                    gtk_widget_get_allocated_width (widget),
                    gtk_widget_get_allocated_height (widget));


  GTK_WIDGET_CLASS (gtk_header_bar_parent_class)->draw (widget, cr);

  return TRUE;
}

static void
gtk_header_bar_class_init (GtkHeaderBarClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  object_class->finalize = gtk_header_bar_finalize;
  object_class->get_property = gtk_header_bar_get_property;
  object_class->set_property = gtk_header_bar_set_property;

  widget_class->size_allocate = gtk_header_bar_size_allocate;
  widget_class->get_preferred_width = gtk_header_bar_get_preferred_width;
  widget_class->get_preferred_height = gtk_header_bar_get_preferred_height;
  widget_class->get_preferred_height_for_width = gtk_header_bar_get_preferred_height_for_width;
  widget_class->get_preferred_width_for_height = gtk_header_bar_get_preferred_width_for_height;
  widget_class->draw = gtk_header_bar_draw;

  container_class->add = gtk_header_bar_add;
  container_class->remove = gtk_header_bar_remove;
  container_class->forall = gtk_header_bar_forall;
  container_class->child_type = gtk_header_bar_child_type;
  container_class->set_child_property = gtk_header_bar_set_child_property;
  container_class->get_child_property = gtk_header_bar_get_child_property;
  container_class->get_path_for_child = gtk_header_bar_get_path_for_child;
  gtk_container_class_handle_border_width (container_class);

  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_PACK_TYPE,
                                              g_param_spec_enum ("pack-type",
                                                                 P_("Pack type"),
                                                                 P_("A GtkPackType indicating whether the child is packed with reference to the start or end of the parent"),
                                                                 GTK_TYPE_PACK_TYPE, GTK_PACK_START,
                                                                 GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_POSITION,
                                              g_param_spec_int ("position",
                                                                P_("Position"),
                                                                P_("The index of the child in the parent"),
                                                                -1, G_MAXINT, 0,
                                                                GTK_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        P_("Title"),
                                                        P_("The title to display"),
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_SUBTITLE,
                                   g_param_spec_string ("subtitle",
                                                        P_("Subtitle"),
                                                        P_("The subtitle to display"),
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_CUSTOM_TITLE,
                                   g_param_spec_object ("custom-title",
                                                        P_("Custom Title"),
                                                        P_("Custom title widget to display"),
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   PROP_SPACING,
                                   g_param_spec_int ("spacing",
                                                     P_("Spacing"),
                                                     P_("The amount of space between children"),
                                                     0, G_MAXINT,
                                                     DEFAULT_SPACING,
                                                     GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_SHOW_CLOSE_BUTTON,
                                   g_param_spec_boolean ("show-close-button",
                                                         P_("Show Close button"),
                                                         P_("Whether to show a window close button"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_PANEL);
}

static void
gtk_header_bar_buildable_add_child (GtkBuildable *buildable,
                                    GtkBuilder   *builder,
                                    GObject      *child,
                                    const gchar  *type)
{
  if (type && strcmp (type, "title") == 0)
    gtk_header_bar_set_custom_title (GTK_HEADER_BAR (buildable), GTK_WIDGET (child));
  else if (!type)
    gtk_container_add (GTK_CONTAINER (buildable), GTK_WIDGET (child));
  else
    GTK_BUILDER_WARN_INVALID_CHILD_TYPE (GTK_HEADER_BAR (buildable), type);
}

static void
gtk_header_bar_buildable_init (GtkBuildableIface *iface)
{
  iface->add_child = gtk_header_bar_buildable_add_child;
}

/**
 * gtk_header_bar_pack_start:
 * @bar: A #GtkHeaderBar
 * @child: the #GtkWidget to be added to @bar
 *
 * Adds @child to @box, packed with reference to the
 * start of the @box.
 *
 * Since: 3.10
 */
void
gtk_header_bar_pack_start (GtkHeaderBar *bar,
                           GtkWidget    *child)
{
  gtk_header_bar_pack (bar, child, GTK_PACK_START);
}

/**
 * gtk_header_bar_pack_end:
 * @bar: A #GtkHeaderBar
 * @child: the #GtkWidget to be added to @bar
 *
 * Adds @child to @box, packed with reference to the
 * end of the @box.
 *
 * Since: 3.10
 */
void
gtk_header_bar_pack_end (GtkHeaderBar *bar,
                         GtkWidget    *child)
{
  gtk_header_bar_pack (bar, child, GTK_PACK_END);
}

/**
 * gtk_header_bar_new:
 *
 * Creates a new #GtkHeaderBar widget.
 *
 * Returns: a new #GtkHeaderBar
 *
 * Since: 3.10
 */
GtkWidget *
gtk_header_bar_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_HEADER_BAR, NULL));
}

/**
 * gtk_header_bar_get_show_close_button:
 * @bar: a #GtkHeaderBar
 *
 * Returns whether this header bar shows a window close
 * button.
 *
 * Returns: %TRUE if a window close button is shown
 *
 * Since: 3.10
 */
gboolean
gtk_header_bar_get_show_close_button (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv;

  g_return_val_if_fail (GTK_IS_HEADER_BAR (bar), FALSE);

  priv = gtk_header_bar_get_instance_private (bar);

  return priv->close_button != NULL;
}

/**
 * gtk_header_bar_set_show_close_button:
 * @bar: a #GtkHeaderBar
 * @setting: %TRUE to show a window close button
 *
 * Sets whether this header bar shows a window close
 * button.
 *
 * Since: 3.10
 */
void
gtk_header_bar_set_show_close_button (GtkHeaderBar *bar,
                                      gboolean      setting)
{
  GtkHeaderBarPrivate *priv;

  g_return_if_fail (GTK_IS_HEADER_BAR (bar));

  priv = gtk_header_bar_get_instance_private (bar);

  setting = setting != FALSE;

  if ((priv->close_button != NULL) == setting)
    return;

  if (setting)
    add_close_button (bar);
  else
    remove_close_button (bar);

  gtk_widget_queue_resize (GTK_WIDGET (bar));

  g_object_notify (G_OBJECT (bar), "show-close-button");
}
