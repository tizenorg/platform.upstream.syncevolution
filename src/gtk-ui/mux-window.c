#include "mux-window.h"
#include "mux-icon-button.h"

static GdkColor mux_window_default_title_bar_bg = { 0, 0x3300, 0x3300, 0x3300 };
static GdkColor mux_window_default_title_bar_fg = { 0, 0x2c00, 0x2c00, 0x2c00 };

#define MUX_WINDOW_DEFAULT_TITLE_BAR_HEIGHT 63

GType
mux_decorations_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GFlagsValue values[] = {
      { MUX_DECOR_CLOSE, "MUX_CLOSE", "close" },
      { MUX_DECOR_SETTINGS, "MUX_SETTINGS", "settings" },
      { 0, NULL, NULL }
    };
    etype = g_flags_register_static (g_intern_static_string ("MuxDecorations"), values);
  }
  return etype;
}


enum {
  PROP_0,
  PROP_DECORATIONS,
};

enum {
  SETTINGS_CLICKED,
  LAST_SIGNAL
};

static guint mux_window_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (MuxWindow, mux_window, GTK_TYPE_WINDOW)

#define GET_PRIVATE(o) \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), MUX_TYPE_WINDOW, MuxWindowPrivate))

typedef struct _MuxWindowPrivate MuxWindowPrivate;

struct _MuxWindowPrivate {
    int dummy;
};

static void
mux_window_get_property (GObject *object, guint property_id,
                         GValue *value, GParamSpec *pspec)
{
    MuxWindow *win = MUX_WINDOW (object);

    switch (property_id) {
    case PROP_DECORATIONS:
        g_value_set_uint (value, win->decorations);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
mux_window_set_property (GObject *object, guint property_id,
                         const GValue *value, GParamSpec *pspec)
{
    MuxWindow *win = MUX_WINDOW (object);

    switch (property_id) {
    case PROP_DECORATIONS:
        mux_window_set_decorations (win, g_value_get_uint (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
mux_window_update_style (MuxWindow *win)
{
    GdkColor *title_bar_bg = NULL;
    GdkColor *title_bar_fg = NULL;
    guint title_bar_height;
    char *title_bar_font = NULL;

    g_return_if_fail (win->title_bar && win->title_label);

    gtk_widget_style_get (GTK_WIDGET (win),
                          "title-bar-height", &title_bar_height, 
                          "title-bar-bg", &title_bar_bg,
                          "title-bar-fg", &title_bar_fg,
                          "title-bar-font", &title_bar_font,
                          NULL);

    if (title_bar_bg) {
        gtk_widget_modify_bg (win->title_bar, GTK_STATE_NORMAL, title_bar_bg);
        gdk_color_free (title_bar_bg);
    } else {
        gtk_widget_modify_bg (win->title_bar, GTK_STATE_NORMAL, 
                              &mux_window_default_title_bar_bg);
    }

    if (title_bar_fg) {
        gtk_widget_modify_fg (win->title_label, GTK_STATE_NORMAL, title_bar_fg);
        gdk_color_free (title_bar_fg);
    } else {
        gtk_widget_modify_fg (win->title_label, GTK_STATE_NORMAL, 
                              &mux_window_default_title_bar_fg);
    }

    if (title_bar_font) {
        PangoFontDescription *desc;
        desc = pango_font_description_from_string (title_bar_font);
        gtk_widget_modify_font (win->title_label, desc);
        pango_font_description_free (desc);
        g_free (title_bar_font);
    }
    
    gtk_widget_set_size_request (win->title_bar, -1, title_bar_height);
    gtk_misc_set_padding (GTK_MISC (win->title_label), title_bar_height / 3, 0);
}

static void 
mux_window_style_set (GtkWidget *widget,
                      GtkStyle *previous)
{
    MuxWindow *win = MUX_WINDOW (widget);

    mux_window_update_style (win);

    GTK_WIDGET_CLASS (mux_window_parent_class)->style_set (widget, previous);
}

static void
mux_window_dispose (GObject *object)
{
    G_OBJECT_CLASS (mux_window_parent_class)->dispose (object);
}

static void
mux_window_finalize (GObject *object)
{
    G_OBJECT_CLASS (mux_window_parent_class)->finalize (object);
}

static gboolean
mux_window_expose(GtkWidget *widget,
                  GdkEventExpose *event)
{   
    if (GTK_WIDGET_DRAWABLE (widget)) {
        (* GTK_WIDGET_CLASS (mux_window_parent_class)->expose_event) (widget, event);
    }
    return FALSE;
}

static void
mux_window_forall (GtkContainer *container,
                   gboolean include_internals,
                   GtkCallback callback,
                   gpointer callback_data)
{
    MuxWindow *mux_win = MUX_WINDOW (container);
    GtkBin *bin = GTK_BIN (container);

    /* FIXME: call parents forall instead */
    if (bin->child)
        (* callback) (bin->child, callback_data);

    if (mux_win->title_bar)
        (* callback) (mux_win->title_bar, callback_data);
}

static void
mux_window_remove (GtkContainer *container,
                   GtkWidget *child)
{
    MuxWindow *win = MUX_WINDOW (container);

    if (child == win->title_bar) {
        gtk_widget_unparent (win->title_bar);
        win->title_bar = NULL;
        win->title_label = NULL;
    } else {
        GTK_CONTAINER_CLASS (mux_window_parent_class)->remove (container, child);
    }
}

static void
mux_window_size_request (GtkWidget *widget,
                         GtkRequisition *requisition)
{
    MuxWindow *mux_win = MUX_WINDOW (widget);
    GtkBin *bin = GTK_BIN (widget);
    GtkRequisition child_req;
    GtkRequisition title_req;

    child_req.width = child_req.height = 0;
    if (bin->child)
        gtk_widget_size_request (bin->child, &child_req);

    title_req.width = title_req.height = 0;
    if (mux_win->title_bar) {
        gtk_widget_size_request (mux_win->title_bar, &title_req);
    }

    requisition->width = MAX (child_req.width, title_req.width) +
                         2 * (GTK_CONTAINER (widget)->border_width +
                              GTK_WIDGET (widget)->style->xthickness);
    requisition->height = title_req.height + child_req.height +
                          2 * (GTK_CONTAINER (widget)->border_width +
                               GTK_WIDGET (widget)->style->ythickness);
}

static void
mux_window_size_allocate (GtkWidget *widget,
                          GtkAllocation *allocation)
{
    GtkBin *bin = GTK_BIN (widget);
    MuxWindow *mux_win = MUX_WINDOW (widget);
    GtkAllocation child_allocation;
    int xmargin, ymargin, title_height;

    widget->allocation = *allocation;
    xmargin = GTK_CONTAINER (widget)->border_width +
              widget->style->xthickness;
    ymargin = GTK_CONTAINER (widget)->border_width +
              widget->style->ythickness;
    title_height = 0;

    if (mux_win->title_bar) {
        GtkAllocation title_allocation;
        GtkRequisition title_req;
        gtk_widget_get_child_requisition (mux_win->title_bar, &title_req);

        title_height = title_req.height;
        title_allocation.x = allocation->x;
        title_allocation.y = allocation->y;
        title_allocation.width = allocation->width;
        title_allocation.height = title_height;
        gtk_widget_size_allocate (mux_win->title_bar, &title_allocation);

    }

    child_allocation.x = allocation->x + xmargin;
    child_allocation.y = allocation->y + title_height + ymargin;
    child_allocation.width = allocation->width - 2 * xmargin;
    child_allocation.height = allocation->height - (2 * ymargin + title_height);

    if (GTK_WIDGET_MAPPED (widget) &&
        (child_allocation.x != mux_win->child_allocation.x ||
         child_allocation.y != mux_win->child_allocation.y ||
         child_allocation.width != mux_win->child_allocation.width ||
         child_allocation.height != mux_win->child_allocation.height)) {
        gdk_window_invalidate_rect (widget->window, &widget->allocation, FALSE);
    }

    if (bin->child && GTK_WIDGET_VISIBLE (bin->child)) {
        gtk_widget_size_allocate (bin->child, &child_allocation);
    }

    mux_win->child_allocation = child_allocation;
}


static void
mux_window_class_init (MuxWindowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
    GParamSpec *pspec;

    g_type_class_add_private (klass, sizeof (MuxWindowPrivate));

    object_class->get_property = mux_window_get_property;
    object_class->set_property = mux_window_set_property;
    object_class->dispose = mux_window_dispose;
    object_class->finalize = mux_window_finalize;

    widget_class->expose_event = mux_window_expose;
    widget_class->size_request = mux_window_size_request;
    widget_class->size_allocate = mux_window_size_allocate;
    widget_class->style_set = mux_window_style_set;

    container_class->forall = mux_window_forall;
    container_class->remove = mux_window_remove;

    pspec = g_param_spec_uint ("title-bar-height",
                               "Title bar height",
                               "Total height of the title bar",
                               0, G_MAXUINT, MUX_WINDOW_DEFAULT_TITLE_BAR_HEIGHT,
                               G_PARAM_READWRITE);
    gtk_widget_class_install_style_property(widget_class, pspec);
    pspec = g_param_spec_boxed ("title-bar-bg",
                                "Title bar bg color",
                                "Color of the title bar background",
                                GDK_TYPE_COLOR,
                                G_PARAM_READWRITE);
    gtk_widget_class_install_style_property(widget_class, pspec);
    pspec = g_param_spec_boxed ("title-bar-fg",
                                "Title bar fg color",
                                "Color of the title bar foreground (text)",
                                GDK_TYPE_COLOR,
                                G_PARAM_READWRITE);
    gtk_widget_class_install_style_property(widget_class, pspec);
    pspec = g_param_spec_string ("title-bar-font",
                                 "Title bar font",
                                 "Pango font description string for title bar text",
                                 "Bold 25",
                                 G_PARAM_READWRITE);
    gtk_widget_class_install_style_property(widget_class, pspec);

    pspec = g_param_spec_flags ("decorations", 
                               NULL,
                               "Bitfield of MuxDecorations defining used window decorations",
                               MUX_TYPE_DECORATIONS, 
                               MUX_DECOR_CLOSE,
                               G_PARAM_READWRITE);
    g_object_class_install_property (object_class,
                                     PROP_DECORATIONS,
                                     pspec);

    mux_window_signals[SETTINGS_CLICKED] = 
            g_signal_new ("settings-clicked",
                          G_OBJECT_CLASS_TYPE (object_class),
                          G_SIGNAL_RUN_FIRST,
                          G_STRUCT_OFFSET (MuxWindowClass, settings_clicked),
                          NULL, NULL,
                          g_cclosure_marshal_VOID__VOID,
                          G_TYPE_NONE, 0, NULL);

}

static void
mux_window_settings_clicked (MuxWindow *window)
{
    g_signal_emit (window, mux_window_signals[SETTINGS_CLICKED], 0, NULL);
}

static void
mux_window_close_clicked (MuxWindow *window)
{
    /* this is how GtkDialog does it... */
    GdkEvent *event;

    event = gdk_event_new (GDK_DELETE);
    event->any.window = g_object_ref (gtk_widget_get_window (GTK_WIDGET (window)));
    event->any.send_event = TRUE;

    gtk_main_do_event (event);
    gdk_event_free (event);
}

static void
mux_window_build_title_bar (MuxWindow *window)
{
    GtkWidget *box, *btn, *sep;
    GdkPixbuf *pixbuf, *pixbuf_hover;
    
    if (window->title_bar) {
        gtk_widget_unparent (window->title_bar);
    }

    window->title_bar = gtk_event_box_new ();
    gtk_widget_set_name (window->title_bar, "mux_window_title_bar");
    gtk_widget_set_parent (window->title_bar, GTK_WIDGET (window));
    gtk_widget_show (window->title_bar);

    box = gtk_hbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (window->title_bar), box);
    gtk_widget_show (box);

    window->title_label = gtk_label_new (gtk_window_get_title (GTK_WINDOW (window)));
    gtk_box_pack_start (GTK_BOX (box), window->title_label,
                        FALSE, FALSE, 0);
    gtk_widget_show (window->title_label);

    if (window->decorations & MUX_DECOR_CLOSE) {
        /* TODO load icons from theme when they are added to it */
        pixbuf = gdk_pixbuf_new_from_file (THEMEDIR "close.png", NULL);
        pixbuf_hover = gdk_pixbuf_new_from_file (THEMEDIR "close_hover.png", NULL);
        btn = g_object_new (MUX_TYPE_ICON_BUTTON,
                            "normal-state-pixbuf", pixbuf,
                            "prelight-state-pixbuf", pixbuf_hover,
                            NULL);
        gtk_widget_set_name (btn, "mux_icon_button_close");
        g_signal_connect_swapped (btn, "clicked", 
                                  G_CALLBACK (mux_window_close_clicked), window);
        gtk_box_pack_end (GTK_BOX (box), btn, FALSE, FALSE, 0);
        gtk_widget_show (btn);
    }

    if (window->decorations & MUX_DECOR_SETTINGS) {
        sep = gtk_vseparator_new ();
        gtk_box_pack_end (GTK_BOX (box), sep, FALSE, FALSE, 0);
        gtk_widget_show (sep);
    }

    if (window->decorations & MUX_DECOR_SETTINGS) {
        /* TODO load icons from theme when they are added to it */
        pixbuf = gdk_pixbuf_new_from_file (THEMEDIR "settings.png", NULL);
        pixbuf_hover = gdk_pixbuf_new_from_file (THEMEDIR "settings_hover.png", NULL);
        btn = g_object_new (MUX_TYPE_ICON_BUTTON,
                            "normal-state-pixbuf", pixbuf,
                            "prelight-state-pixbuf", pixbuf_hover,
                            NULL);
        gtk_widget_set_name (btn, "mux_icon_button_settings");
        g_signal_connect_swapped (btn, "clicked", 
                                  G_CALLBACK (mux_window_settings_clicked), window);
        gtk_box_pack_end (GTK_BOX (box), btn, FALSE, FALSE, 0);
        gtk_widget_show (btn);
    }

    mux_window_update_style (window);

    gtk_widget_map (window->title_bar); /*TODO: is there a better way to do this ? */
    if (GTK_WIDGET_VISIBLE (window))
        gtk_widget_queue_resize (GTK_WIDGET (window));

}

static void
mux_window_title_changed (MuxWindow *window, 
                          GParamSpec *pspec,
                          gpointer user_data)
{
    if (window->title_label) {
        gtk_label_set_text (GTK_LABEL (window->title_label), 
                            gtk_window_get_title (GTK_WINDOW (window)));
    }
}

static void
mux_window_init (MuxWindow *self)
{
    self->decorations = MUX_DECOR_CLOSE;

    gtk_window_maximize (GTK_WINDOW (self));
    gtk_window_set_decorated (GTK_WINDOW (self), FALSE);

    g_signal_connect (self, "notify::title",
                      G_CALLBACK (mux_window_title_changed), NULL);

    mux_window_build_title_bar (self);
}

GtkWidget*
mux_window_new (void)
{
    return g_object_new (MUX_TYPE_WINDOW, NULL);
}

void 
mux_window_set_decorations (MuxWindow *window, 
                            MuxDecorations decorations)
{
    g_return_if_fail (MUX_IS_WINDOW (window));
    
    if (decorations != window->decorations) {
        window->decorations = decorations;
        mux_window_build_title_bar (window);
    }
}

MuxDecorations 
mux_window_get_decorations (MuxWindow *window)
{
    g_return_val_if_fail (MUX_IS_WINDOW (window), MUX_DECOR_NONE);

    return window->decorations;
}
