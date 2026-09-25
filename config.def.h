/* See LICENSE file for copyright and license details. */
/* Mirrors dwmr/src/config.rs (Config::default) */

/* appearance */
static unsigned int borderpx        = 1;        /* border pixel of windows */
static unsigned int gappih          = 20;       /* horiz inner gap between windows */
static unsigned int gappiv          = 20;       /* vert inner gap between windows */
static unsigned int gappoh          = 20;       /* horiz outer gap between windows and screen edge */
static unsigned int gappov          = 20;       /* vert outer gap between windows and screen edge */
static int smartgaps                = 0;        /* 1 means no outer gap when there is only one window */
static int browsergaps              = 0;        /* 0 means no outer gap when there is only one window and it is firefox */
static unsigned int snap            = 32;       /* snap pixel */
static int swallowfloating          = 0;        /* 1 means swallow floating windows by default */
static int showbar                  = 1;        /* 0 means no bar */
static int topbar                   = 1;        /* 0 means bottom bar */
static const int focusonwheel       = 0;        /* 0 allows the user to scroll window without changing focus */
static const char *fonts[]          = { "monospace:size=10" };
/* bigger font for status text between ^B^ and ^N^, e.g. a block's icon */
static const char *statusbigfonts[] = { "monospace:size=14" };
static const char dmenufont[]       = "monospace:size=10";
static const char col_gray1[]       = "#222222";
static const char col_gray2[]       = "#444444";
static const char col_gray3[]       = "#bbbbbb";
static const char col_gray4[]       = "#eeeeee";
static const char col_cyan[]        = "#005577";
/* the scheme colors, "#RRGGBB": the resources[] below are written into them */
static char normfgcolor[]           = "#bbbbbb";
static char normbgcolor[]           = "#222222";
static char normbordercolor[]       = "#444444";
static char selfgcolor[]            = "#eeeeee";
static char selbgcolor[]            = "#005577";
static char selbordercolor[]        = "#005577";
static const char *colors[][3]      = {
	/*               fg           bg           border   */
	[SchemeNorm] = { normfgcolor, normbgcolor, normbordercolor },
	[SchemeSel]  = { selfgcolor,  selbgcolor,  selbordercolor  },
};
/* status text colors (status2d): the text starts in col1; ^3^..^6^ switch to
 * col3..col6, ^c#rrggbb^ to any color and ^2^ to the weather color from the
 * temperature after it: +20 and above col21, below +20 col22, negative col23,
 * no sign col24 */
static const char col1[]            = "#98971a";
static const char col21[]           = "#fb4934";
static const char col22[]           = "#ebdbb2";
static const char col23[]           = "#458588";
static const char col24[]           = "#ebdbb2";
static const char col3[]            = "#fabd2f";
static const char col4[]            = "#83a598";
static const char col5[]            = "#d3869b";
static const char col6[]            = "#8ec07c";

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

/* scratchpads: each has its own tag bit above the normal tags, SPTAG(i), and
 * a command that togglescratch spawns when no window has that tag yet */
static const char *spcmd1[] = { "st", "-n", "spterm", "-e", "python3", NULL };
static const char *spcmd2[] = { "st", "-n", "spcalc", NULL };
static const Sp scratchpads[] = {
	/* name          cmd  */
	{ "spterm",      spcmd1 },
	{ "spcalc",      spcmd2 },
};

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title           tags mask  isfloating  isterminal  noswallow  monitor */
	{ "Gimp",     NULL,       NULL,           0,         1,          0,          0,         -1 },
	{ "Firefox",  NULL,       NULL,           1 << 8,    0,          0,          0,         -1 },
	{ "wezterm",  NULL,       NULL,           0,         0,          1,          0,         -1 },
	{ NULL,       NULL,       "Event Tester", 0,         0,          0,          1,         -1 }, /* xev */
	{ NULL,       "spterm",   NULL,           SPTAG(0),  1,          1,          1,         -1 },
	{ NULL,       "spcalc",   NULL,           SPTAG(1),  1,          1,          0,         -1 },
};

/* layout(s) */
static float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static int nmaster     = 1;    /* number of clients in master area */
static int resizehints = 1;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 1; /* 1 will force focus on the fullscreen window */
static const int refreshrate = 120;  /* refresh rate (per second) for client move/resize */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[@]",      spiral },    /* first entry is default */
	{ "[]=",      tile },
	{ "TTT",      bstack },
	{ "[\\]",     dwindle },
	{ "[D]",      deck },
	{ "[M]",      monocle },
	{ "|M|",      centeredmaster },
	{ ">M>",      centeredfloatingmaster },
	{ "><>",      NULL },    /* no layout function means floating behavior */
};

/* key definitions */
#define MODKEY Mod1Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tagview,        {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggleview,     {.ui = 1 << TAG} },
/* focusstack/pushstack take a stack position (stacker): INC(n) is relative
 * to the focused window, 0 is the top, -1 the bottom */
#define STACKKEYS(MOD,ACTION) \
	{ MOD,                          XK_j,     ACTION##stack,  {.i = INC(+1) } }, \
	{ MOD,                          XK_k,     ACTION##stack,  {.i = INC(-1) } }, \
	{ MOD|ControlMask,              XK_j,     ACTION##stack,  {.i = -1 } }, \
	{ MOD|ControlMask,              XK_k,     ACTION##stack,  {.i = 0 } },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* the status bar program that sigstatusbar() signals (statuscmd), found by
 * process name; "" disables the clicks */
static const char statusbar[] = "dwmblocksc";

/* the command runautostart() runs once at startup, after the existing
 * windows have been taken over; { NULL } runs none */
static const char *autostart[] = { "sh", "-c", "killall -q dwmblocksc; dwmblocksc &", NULL };

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", col_gray1, "-nf", col_gray3, "-sb", col_cyan, "-sf", col_gray4, NULL };
static const char *termcmd[]  = { "st", NULL };

/*
 * Xresources preferences to load at startup; the same resource may set
 * several values, and sel's fg/bg are deliberately inverted
 */
static const ResourcePref resources[] = {
	{ "color0",             STRING,     &normbordercolor },
	{ "foreground",         STRING,     &selbordercolor },
	{ "color0",             STRING,     &normbgcolor },
	{ "foreground",         STRING,     &normfgcolor },
	{ "color0",             STRING,     &selfgcolor },
	{ "foreground",         STRING,     &selbgcolor },
	{ "borderpx",           INTEGER,    &borderpx },
	{ "snap",               INTEGER,    &snap },
	{ "showbar",            INTEGER,    &showbar },
	{ "topbar",             INTEGER,    &topbar },
	{ "nmaster",            INTEGER,    &nmaster },
	{ "resizehints",        INTEGER,    &resizehints },
	{ "mfact",              FLOAT,      &mfact },
	{ "gappih",             INTEGER,    &gappih },
	{ "gappiv",             INTEGER,    &gappiv },
	{ "gappoh",             INTEGER,    &gappoh },
	{ "gappov",             INTEGER,    &gappov },
	{ "swallowfloating",    INTEGER,    &swallowfloating },
	{ "smartgaps",          INTEGER,    &smartgaps },
};

static const Key keys[] = {
	/* modifier                     key        function        argument */
	{ MODKEY,                       XK_p,      spawn,          {.v = dmenucmd } },
	{ MODKEY|ShiftMask,             XK_Return, spawn,          {.v = termcmd } },
	{ MODKEY,                       XK_b,      togglebar,      {0} },
	STACKKEYS(MODKEY,                          focus)
	STACKKEYS(MODKEY|ShiftMask,                push)
	{ MODKEY,                       XK_i,      incnmaster,     {.i = +1 } },
	{ MODKEY,                       XK_d,      incnmaster,     {.i = -1 } },
	{ MODKEY,                       XK_y,      setmfact,       {.f = -0.05} },
	{ MODKEY,                       XK_o,      setmfact,       {.f = +0.05} },
	{ MODKEY,                       XK_x,      defaultgaps,    {0} },
	{ MODKEY,                       XK_z,      togglegaps,     {0} },
	{ MODKEY|ControlMask,           XK_z,      togglebgaps,    {0} },
	{ MODKEY,                       XK_plus,   incrgaps,       {.i = +3 } },
	{ MODKEY,                       XK_minus,  incrgaps,       {.i = -3 } },
	{ MODKEY|ShiftMask,             XK_plus,   incrgaps,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_minus,  incrgaps,       {.i = -1 } },
	{ MODKEY,                       XK_Return, zoom,           {0} },
	{ MODKEY,                       XK_Tab,    shiftviewclients, {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_Tab,    shiftviewclients, {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_c,      killclient,     {0} },
	{ MODKEY,                       XK_t,      setlayout,      {.v = &layouts[1]} },
	{ MODKEY,                       XK_f,      setlayout,      {.v = &layouts[8]} },
	{ MODKEY,                       XK_m,      setlayout,      {.v = &layouts[5]} },
	{ MODKEY,                       XK_space,  setlayout,      {0} },
	{ MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },
	{ MODKEY|ShiftMask,             XK_f,      togglefullscr,  {0} },
	{ MODKEY|ShiftMask,             XK_less,   togglesticky,   {0} },
	{ MODKEY|ShiftMask,             XK_y,      shifttag,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_o,      shifttag,       {.i = -1 } },
	{ MODKEY,                       XK_0,      view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },
	{ MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_period, focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },
	{ MODKEY,                       XK_h,      focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_l,      focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_h,      tagmonview,     {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_l,      tagmonview,     {.i = +1 } },
	{ MODKEY|ControlMask,           XK_h,      tagmon,         {.i = -1 } },
	{ MODKEY|ControlMask,           XK_l,      tagmon,         {.i = +1 } },
	{ MODKEY,                       XK_Left,   focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_Right,  focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_Left,   tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_Right,  tagmon,         {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_p,      togglebars,     {0} },
	{ MODKEY|ControlMask|ShiftMask, XK_p,      togglebar,      {0} },
	{ MODKEY,                       XK_apostrophe, togglescratch, {.ui = 0 } },
	{ MODKEY|ShiftMask,             XK_apostrophe, togglescratch, {.ui = 1 } },
	TAGKEYS(                        XK_1,                      0)
	TAGKEYS(                        XK_2,                      1)
	TAGKEYS(                        XK_3,                      2)
	TAGKEYS(                        XK_4,                      3)
	TAGKEYS(                        XK_5,                      4)
	TAGKEYS(                        XK_6,                      5)
	TAGKEYS(                        XK_7,                      6)
	TAGKEYS(                        XK_8,                      7)
	TAGKEYS(                        XK_9,                      8)
	{ MODKEY|ShiftMask,             XK_q,      quit,           {0} },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[5]} },
	{ ClkStatusText,        0,              Button1,        sigstatusbar,   {.i = 1} },
	{ ClkStatusText,        0,              Button2,        sigstatusbar,   {.i = 2} },
	{ ClkStatusText,        0,              Button3,        sigstatusbar,   {.i = 3} },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        defaultgaps,    {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkClientWin,         MODKEY,         Button4,        incrgaps,       {.i = +1} },
	{ ClkClientWin,         MODKEY,         Button5,        incrgaps,       {.i = -1} },
	{ ClkRootWin,           0,              Button2,        togglebar,      {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
	{ ClkTagBar,            0,              Button4,        shiftview,      {.i = -1} },
	{ ClkTagBar,            0,              Button5,        shiftview,      {.i = 1} },
};
