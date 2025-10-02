#include <cstdlib>
#include <cstdio>
#include <locale.h>
#include <string>

#include <X11/Xlib.h>        // `apt-get install libx11-dev`
#include <X11/Xmu/WinUtil.h> // `apt-get install libxmu-dev`

Bool xerror = False;

Display *open_display()
{
    Display *d = XOpenDisplay(nullptr);
    if (d == nullptr)
    {
        printf("fail\n");
        exit(EXIT_FAILURE);
    }
    return d;
}

int handle_error([[maybe_unused]] Display *display, [[maybe_unused]] XErrorEvent *error)
{
    printf("ERROR: X11 error\n");
    xerror = True;
    return 1;
}

Window get_focus_window(Display *d)
{
    Window w;
    int revert_to;
    XGetInputFocus(d, &w, &revert_to); // see man
    if (xerror)
    {
        printf("fail\n");
        return 0;
    }
    else if (w == None)
    {
        printf("no focus window\n");
        return 0;
    }
    else
    {
        return w;
    }
}

// get the top window.
// a top window have the following specifications.
//  * the start window is contained the descendent windows.
//  * the parent window is the root window.
Window get_top_window(Display *d, Window start)
{
    Window w = start;
    Window parent = start;
    Window root = None;
    Window *children;
    unsigned int nchildren;
    Status s;
    while (parent != root)
    {
        w = parent;
        s = XQueryTree(d, w, &root, &parent, &children, &nchildren); // see man

        if (s)
            XFree(children);

        if (xerror)
        {
            printf("fail\n");
            return 0;
        }
    }
    return w;
}

// search a named window (that has a WM_STATE prop)
// on the descendent windows of the argment Window.
Window get_named_window(Display *d, Window start)
{
    Window w;
    w = XmuClientWindow(d, start); // see man
    return w;
}

std::string print_window_class(Display *d, Window w)
{
    Status s;
    XClassHint *clas;

    clas = XAllocClassHint(); // see man
    if (!clas)
    {
        printf("ERROR: XAllocClassHint\n");
        XCloseDisplay(d);
        return "E";
    }

    if (xerror)
    {
        printf("ERROR: XAllocClassHint\n");
        XFree(clas);
        XCloseDisplay(d);
        return "E";
    }

    s = XGetClassHint(d, w, clas); // see man
    std::string result = "E";
    if (!xerror && s && clas->res_class)
    {
        result = clas->res_class;
    }
    else
    {
        printf("ERROR: XGetClassHint\n");
    }

    if (clas->res_name)
        XFree(clas->res_name);
    if (clas->res_class)
        XFree(clas->res_class);
    XFree(clas);
    XCloseDisplay(d);
    return result;
}

std::string getActiveWindow()
{
    Display *d;
    Window w;

    // for XmbTextPropertyToTextList
    setlocale(LC_ALL, ""); // see man locale

    d = open_display();
    XSetErrorHandler(handle_error);

    // get active window
    w = get_focus_window(d);
    w = get_top_window(d, w);
    w = get_named_window(d, w);
    return print_window_class(d, w);
}