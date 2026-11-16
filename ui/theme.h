#ifndef THEME_H
#define THEME_H

#include <QtWidgets>
#include <functional>

// ============================================================================
// PlaceBridge visual theme (v3).
//
// Reference point: the sidebar admin dashboards of Stripe / Linear — a quiet
// neutral canvas, one accent used sparingly, dense readable tables, and
// hierarchy carried by weight and spacing rather than by colour and ornament.
//
// Neutrals are biased very slightly green so they sit with the accent instead of
// reading as default grey. Accent is a deep emerald; amber is kept only as a
// secondary/attention colour, which is the one thread back to the old identity.
// Type is a single sans family across the whole app: hierarchy comes from the
// size/weight scale below, not from mixing a serif in.
//
//   canvas #F6F8F7   surface #FFFFFF   accent #0E7C66   attention #C2710C
// ============================================================================
namespace Theme {

struct Tokens {
    QString primary, primaryDark, accent, background, surface, border;
    QString textMuted, success, warning, danger, text;
    QString surfaceAlt, accentSoft, accentText, sidebar, sidebarText, focus;
};

inline Tokens readTokens() {
    QSettings settings;
    const QString preset = settings.value("theme/preset", "Emerald").toString();
    const bool dark = settings.value("theme/mode", "Light").toString() == "Dark";

    // Each preset only moves the accent family; the neutral ramp is shared so
    // every preset keeps the same contrast behaviour in both modes.
    QString accent = "#0E7C66", accentDark = "#0B6554", accentSoft = "#E6F2EE", accentText = "#0A5044";
    if (preset == "Indigo") {
        accent = "#4F46E5"; accentDark = "#4338CA"; accentSoft = "#EAE9FB"; accentText = "#3730A3";
    } else if (preset == "Slate") {
        accent = "#334155"; accentDark = "#1E293B"; accentSoft = "#E9EDF2"; accentText = "#1E293B";
    }
    if (dark) {
        if (preset == "Indigo") { accent = "#8B84F7"; accentDark = "#A5A0FA"; accentSoft = "#262546"; accentText = "#C9C5FD"; }
        else if (preset == "Slate") { accent = "#A9B6C7"; accentDark = "#C3CDD9"; accentSoft = "#232A33"; accentText = "#D6DEE7"; }
        else { accent = "#2DBE9B"; accentDark = "#4ED4B3"; accentSoft = "#15302A"; accentText = "#7FE3CC"; }
    }

    Tokens value;
    value.accent = accent;
    value.primary = accent;            // one accent: "primary" and "accent" are the same hue now
    value.primaryDark = accentDark;
    value.accentSoft = accentSoft;
    value.accentText = accentText;
    if (dark) {
        value.background  = "#0E1513";
        value.surface     = "#16211E";
        value.surfaceAlt  = "#1C2825";
        value.border      = "#2A3A35";
        value.text        = "#E8EFEC";
        value.textMuted   = "#94A8A2";
        value.sidebar     = "#111B18";
        value.sidebarText = "#C7D6D1";
        value.success     = "#3DBE80";
        value.warning     = "#D8A33C";
        value.danger      = "#E8705F";
        value.focus       = accent;
    } else {
        value.background  = "#F6F8F7";
        value.surface     = "#FFFFFF";
        value.surfaceAlt  = "#F1F4F3";
        value.border      = "#E2E8E5";
        value.text        = "#10211D";
        value.textMuted   = "#5F726C";
        value.sidebar     = "#FFFFFF";
        value.sidebarText = "#3C4E49";
        value.success     = "#117A4B";
        value.warning     = "#9A6206";
        value.danger      = "#B4342A";
        value.focus       = accent;
    }
    return value;
}

// Reading QSettings on every call showed up in paintEvent() paths, so the active
// token set is cached and dropped whenever the theme is (re)applied.
inline Tokens &tokenCache() {
    static Tokens cached;
    return cached;
}

inline bool &tokensCached() {
    static bool loaded = false;
    return loaded;
}

inline Tokens tokens() {
    if (!tokensCached()) {
        tokenCache() = readTokens();
        tokensCached() = true;
    }
    return tokenCache();
}

inline void invalidateTokens() { tokensCached() = false; }

inline bool isDark() {
    return QSettings().value("theme/mode", "Light").toString() == "Dark";
}

// ------------------------------------------------------------------ fonts
inline void loadBundledFonts() {
    const QString cwd = QDir::currentPath();
    const QString app = QCoreApplication::applicationDirPath();
    const QStringList bases = { cwd + "/fonts", cwd + "/../fonts", app + "/fonts",
                                app + "/../fonts", app + "/../../fonts" };
    for (const QString &base : bases) {
        QDir d(base);
        if (!d.exists()) continue;
        const QStringList files = d.entryList({ "*.ttf", "*.otf" }, QDir::Files);
        for (const QString &f : files) QFontDatabase::addApplicationFont(d.absoluteFilePath(f));
    }
}

inline QString serif() {
    static const QString f = QFontDatabase::families().contains("Fraunces")
                                 ? QStringLiteral("Fraunces") : QStringLiteral("Georgia");
    return f;
}

inline QString sans() {
    static const QString f = [] {
        const QStringList fams = QFontDatabase::families();
        for (const char *c : { "Plus Jakarta Sans", "Manrope", "Segoe UI", "Helvetica Neue", "Arial" })
            if (fams.contains(QString::fromLatin1(c))) return QString::fromLatin1(c);
        return QString("Sans Serif");
    }();
    return f;
}

// ------------------------------------------------------------ style sheet
inline QString pillRule(const QString &tone, const QString &bg, const QString &fg, const QString &bd) {
    return QString("QLabel[pill=\"%1\"] { background:%2; color:%3; border:1px solid %4; "
                   "border-radius:6px; padding:3px 9px; font-size:11px; font-weight:700; }\n")
        .arg(tone, bg, fg, bd);
}

// Mixes two hex colours; used to derive hover/soft tints from the tokens instead
// of hard-coding a second palette.
inline QString mix(const QString &a, const QString &b, double t) {
    const QColor x(a), y(b);
    return QColor::fromRgbF(x.redF() + (y.redF() - x.redF()) * t,
                            x.greenF() + (y.greenF() - x.greenF()) * t,
                            x.blueF() + (y.blueF() - x.blueF()) * t).name();
}

// ---------------------------------------------------------------------------
// The sheet is written with @NAME@ placeholders and substituted from the active
// tokens. The previous version search-and-replaced raw hex values, which meant
// any literal that happened to match a token changed colour too.
// ---------------------------------------------------------------------------
inline QString stylesheet() {
    const Tokens c = tokens();
    const bool dark = isDark();
    const QString hover     = mix(c.surface, c.text, dark ? 0.08 : 0.04);
    const QString hoverHard = mix(c.surface, c.text, dark ? 0.14 : 0.07);
    const QString rowHover  = mix(c.surface, c.accent, dark ? 0.10 : 0.05);
    const QString shadowFg  = mix(c.background, c.text, 0.18);

    QString q = R"QSS(
/* ------------------------------------------------------------------- base */
QWidget { color:@TEXT@; }
QToolTip { background:@TEXTC@; color:@SURFACE@; border:none; padding:6px 9px;
           border-radius:6px; font-size:12px; }
QDialog, QMessageBox { background:@BG@; }
QScrollArea { background:@BG@; border:none; }
QWidget#pageBody, QWidget#canvas { background:@BG@; }
QMenu { background:@SURFACE@; color:@TEXT@; border:1px solid @BORDER@; border-radius:8px; padding:4px; }
QMenu::item { padding:7px 22px 7px 12px; border-radius:5px; }
QMenu::item:selected { background:@ACCENTSOFT@; color:@ACCENTTEXT@; }
QSplitter::handle { background:@BORDER@; }
QSplitter::handle:horizontal { width:1px; }
QSplitter::handle:vertical { height:1px; }

QScrollBar:vertical { background:transparent; width:11px; margin:2px; }
QScrollBar::handle:vertical { background:@BORDER@; border-radius:5px; min-height:34px; }
QScrollBar::handle:vertical:hover { background:@MUTED@; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:transparent; }
QScrollBar:horizontal { background:transparent; height:11px; margin:2px; }
QScrollBar::handle:horizontal { background:@BORDER@; border-radius:5px; min-width:34px; }
QScrollBar::handle:horizontal:hover { background:@MUTED@; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background:transparent; }

/* ---------------------------------------------------------------- sidebar */
QFrame#sidebar { background:@SIDEBAR@; border-right:1px solid @BORDER@; }
QLabel#brandName { color:@TEXT@; font-size:15px; font-weight:800; letter-spacing:-0.2px; }
QLabel#brandSub { color:@MUTED@; font-size:9px; font-weight:800; letter-spacing:1.1px; }
QLabel#sideSection { color:@MUTED@; font-size:9px; font-weight:800; letter-spacing:1.2px;
                     padding:2px 10px; }
QPushButton[kind="side"] { background:transparent; color:@SIDEBARTEXT@; border:none;
                           border-radius:7px; padding:8px 11px; font-size:13px; font-weight:600;
                           text-align:left; }
QPushButton[kind="side"]:hover { background:@HOVER@; color:@TEXT@; }
QPushButton[kind="side"]:checked { background:@ACCENTSOFT@; color:@ACCENTTEXT@; font-weight:700; }
QPushButton[kind="side"]:focus { border:1px solid @ACCENT@; }

/* ----------------------------------------------------------------- topbar */
QFrame#topbar { background:@SURFACE@; border-bottom:1px solid @BORDER@; }
QLabel#pageTitle { color:@TEXT@; font-size:19px; font-weight:800; letter-spacing:-0.3px; }
QLabel#pageCrumb { color:@MUTED@; font-size:11px; font-weight:600; }
QLabel#rolePill { background:@ACCENTSOFT@; color:@ACCENTTEXT@; border:1px solid @BORDER@;
                  border-radius:6px; padding:3px 9px; font-size:11px; font-weight:700; }
QLabel#userName { color:@TEXT@; font-weight:650; font-size:12px; }
QLabel#avatar { background:@ACCENT@; color:#FFFFFF; border-radius:15px; font-weight:800; font-size:12px; }

/* ------------------------------------------------------------ page header */
QFrame#pageHead { background:transparent; border:none; border-bottom:1px solid @BORDER@; }
QLabel#headTag { color:@MUTED@; font-size:10px; font-weight:800; letter-spacing:1.1px; }
QLabel#headTitle { color:@TEXT@; font-size:23px; font-weight:800; letter-spacing:-0.4px; }
QLabel#headSub { color:@MUTED@; font-size:13px; font-weight:450; }

/* ------------------------------------------------------------------ cards */
QFrame#softCard { background:@SURFACEALT@; border:1px solid @BORDER@; border-radius:9px; }
QFrame#surfaceCard { background:@SURFACE@; border:1px solid @BORDER@; border-radius:8px; }
QFrame#emptyState { background:@SURFACEALT@; border:1px dashed @BORDER@; border-radius:9px; }
QLabel#emptyStateTitle { font-size:13px; font-weight:750; color:@TEXT@; }
QLabel#emptyStateDetail { color:@MUTED@; font-size:12px; }
QFrame#divider { background:@BORDER@; border:none; max-height:1px; min-height:1px; }

QLabel[role="h2"] { font-size:15px; font-weight:750; color:@TEXT@; letter-spacing:-0.2px; }
QLabel[role="h3"] { font-size:13px; font-weight:700; color:@TEXT@; }
QLabel[role="muted"] { color:@MUTED@; font-size:12px; }
QLabel[role="eyebrow"] { color:@MUTED@; font-size:10px; font-weight:750; letter-spacing:0.9px; }
QLabel[role="statValue"] { font-size:28px; font-weight:800; color:@TEXT@; letter-spacing:-0.8px; }
QLabel[role="value"] { font-size:13px; font-weight:650; color:@TEXT@; }
QLabel[role="bigName"] { font-size:20px; font-weight:800; color:@TEXT@; letter-spacing:-0.4px; }
QLabel[role="skill"] { background:@SURFACEALT@; color:@TEXT@; border:1px solid @BORDER@;
                       border-radius:6px; padding:3px 9px; font-size:11px; font-weight:600; }
QLabel[role="error"] { color:@DANGER@; background:@DANGERSOFT@; border:1px solid @DANGERLINE@;
                       border-radius:7px; padding:8px 11px; font-size:12px; }

/* ---------------------------------------------------------------- buttons */
QPushButton { font-size:13px; }
QPushButton[kind="primary"] { background:@ACCENT@; color:#FFFFFF; border:1px solid @ACCENT@;
                              border-radius:7px; padding:8px 15px; font-weight:650; }
QPushButton[kind="primary"]:hover { background:@ACCENTDARK@; border-color:@ACCENTDARK@; }
QPushButton[kind="primary"]:disabled { background:@SURFACEALT@; color:@MUTED@; border-color:@BORDER@; }
QPushButton[kind="soft"] { background:@SURFACE@; color:@TEXT@; border:1px solid @BORDER@;
                           border-radius:7px; padding:7px 13px; font-weight:600; }
QPushButton[kind="soft"]:hover { background:@HOVER@; border-color:@MUTED@; }
QPushButton[kind="soft"]:disabled { background:@SURFACEALT@; color:@MUTED@; border-color:@BORDER@; }
QPushButton[kind="ghost"] { background:transparent; color:@TEXT@; border:1px solid @BORDER@;
                            border-radius:7px; padding:7px 13px; font-weight:600; }
QPushButton[kind="ghost"]:hover { background:@HOVER@; }
QPushButton[kind="danger"] { background:@SURFACE@; color:@DANGER@; border:1px solid @DANGERLINE@;
                             border-radius:7px; padding:7px 13px; font-weight:650; }
QPushButton[kind="danger"]:hover { background:@DANGERSOFT@; }
QPushButton[kind="done"] { background:@SUCCESSSOFT@; color:@SUCCESS@; border:1px solid @SUCCESSLINE@;
                           border-radius:7px; padding:7px 13px; font-weight:650; }
QPushButton[kind="link"] { background:transparent; color:@ACCENT@; border:none; font-weight:650;
                           padding:3px 0; text-align:left; }
QPushButton[kind="link"]:hover { color:@ACCENTDARK@; text-decoration:underline; }
QPushButton[kind="chip"] { background:@SURFACE@; color:@MUTED@; border:1px solid @BORDER@;
                           border-radius:7px; padding:6px 13px; font-weight:600; font-size:12px; }
QPushButton[kind="chip"]:hover { background:@HOVER@; color:@TEXT@; }
QPushButton[kind="chip"]:checked { background:@ACCENTSOFT@; color:@ACCENTTEXT@; border-color:@ACCENT@;
                                   font-weight:700; }
QPushButton[kind="role"] { background:@SURFACE@; color:@MUTED@; border:1px solid @BORDER@;
                           border-radius:8px; padding:11px 8px; font-weight:650; font-size:12px; }
QPushButton[kind="role"]:hover { border-color:@ACCENT@; color:@TEXT@; }
QPushButton[kind="role"]:checked { background:@ACCENTSOFT@; border:1px solid @ACCENT@; color:@ACCENTTEXT@; }
QPushButton[kind="nav"] { background:transparent; color:@SIDEBARTEXT@; border:1px solid transparent;
                          border-radius:7px; padding:7px 13px; font-weight:650; font-size:12px; }
QPushButton[kind="nav"]:hover { background:@HOVER@; color:@TEXT@; }
QPushButton[kind="nav"]:checked { background:@ACCENTSOFT@; color:@ACCENTTEXT@; border-color:@ACCENT@; }
QPushButton[hasUnread="true"] { background:@ACCENT@; color:#FFFFFF; border-color:@ACCENT@; font-weight:750; }
QPushButton[hasUnread="true"]:hover { background:@ACCENTDARK@; }
QPushButton[kind="primary"]:focus, QPushButton[kind="soft"]:focus, QPushButton[kind="ghost"]:focus,
QPushButton[kind="danger"]:focus, QPushButton[kind="chip"]:focus, QPushButton[kind="nav"]:focus {
    border:2px solid @ACCENT@; }
QDialogButtonBox QPushButton, QMessageBox QPushButton { background:@ACCENT@; color:#FFFFFF; border:none;
                          border-radius:7px; padding:7px 16px; min-width:74px; font-weight:650; }
QDialogButtonBox QPushButton:hover, QMessageBox QPushButton:hover { background:@ACCENTDARK@; }

/* ----------------------------------------------------------------- inputs */
QLineEdit, QComboBox, QDateEdit, QTimeEdit, QDoubleSpinBox, QSpinBox, QTextEdit {
    background:@SURFACE@; color:@TEXT@; border:1px solid @BORDER@; border-radius:7px;
    padding:7px 10px; font-size:13px;
    selection-background-color:@ACCENT@; selection-color:#FFFFFF; }
QLineEdit:hover, QComboBox:hover, QTextEdit:hover { border-color:@MUTED@; }
QLineEdit:focus, QComboBox:focus, QDateEdit:focus, QTimeEdit:focus,
QDoubleSpinBox:focus, QSpinBox:focus, QTextEdit:focus { border:1px solid @ACCENT@; }
QLineEdit::placeholder { color:@MUTED@; }
QComboBox::drop-down { border:none; width:22px; }
QComboBox QAbstractItemView { background:@SURFACE@; color:@TEXT@; border:1px solid @BORDER@;
    border-radius:7px; selection-background-color:@ACCENTSOFT@; selection-color:@ACCENTTEXT@; }
QCheckBox { spacing:7px; color:@TEXT@; font-size:12px; }
QCheckBox::indicator { width:15px; height:15px; border:1px solid @MUTED@; border-radius:4px;
                       background:@SURFACE@; }
QCheckBox::indicator:checked { background:@ACCENT@; border-color:@ACCENT@; }
QFormLayout QLabel { color:@MUTED@; font-size:12px; font-weight:600; }

/* --------------------------------------------------------------- progress */
QProgressBar { background:@SURFACEALT@; border:none; border-radius:3px; }
QProgressBar::chunk { background:@ACCENT@; border-radius:3px; }
QProgressBar[tone="forest"]::chunk { background:@ACCENT@; }
QProgressBar[tone="amber"]::chunk { background:@WARNING@; }
QProgressBar[tone="danger"]::chunk { background:@DANGER@; }

/* ----------------------------------------------------------------- tables */
QTableWidget { background:transparent; border:none; gridline-color:transparent; outline:0;
               font-size:13px; }
QTableWidget::item { padding:6px 10px; border-bottom:1px solid @BORDER@; }
QTableWidget::item:hover { background:@ROWHOVER@; }
QTableWidget::item:selected { background:@ACCENTSOFT@; color:@TEXT@; }
QHeaderView { background:transparent; }
QHeaderView::section { background:@SURFACEALT@; color:@MUTED@; border:none;
                       border-bottom:1px solid @BORDER@; padding:8px 10px; font-size:10px;
                       font-weight:750; letter-spacing:0.7px; }
QHeaderView::section:hover { color:@TEXT@; background:@HOVER@; }
QListWidget { background:@SURFACE@; color:@TEXT@; border:1px solid @BORDER@; border-radius:7px;
              padding:3px; outline:none; font-size:13px; }
QListWidget::item { padding:7px 8px; border-radius:5px; }
QListWidget::item:hover { background:@HOVER@; }
QListWidget::item:selected { background:@ACCENTSOFT@; color:@ACCENTTEXT@; }

/* --------------------------------------------------------------- calendar */
QFrame#dayCell { background:@SURFACE@; border:1px solid @BORDER@; }
QFrame#dayCell[today="true"] { background:@ACCENTSOFT@; border:1px solid @ACCENT@; }
QFrame#dayCell[other="true"] { background:@SURFACEALT@; }
QLabel[role="dow"] { color:@MUTED@; font-size:10px; font-weight:750; letter-spacing:0.9px; }
QLabel[role="dayNum"] { color:@MUTED@; font-size:12px; font-weight:600; }
QLabel[role="dayNumToday"] { color:@ACCENTTEXT@; font-size:13px; font-weight:800; }
QLabel[role="dayNumOther"] { color:@MUTED@; font-size:12px; }

/* ------------------------------------------------------------------ toast */
QLabel#toast { background:@TEXTC@; color:@SURFACE@; border-left:3px solid @ACCENT@;
               border-radius:8px; padding:11px 16px; font-weight:650; font-size:12px; }
QLabel#toast[tone="success"] { border-left:3px solid @SUCCESS@; }
QLabel#toast[tone="danger"] { border-left:3px solid @DANGER@; }
QLabel#toast[tone="info"] { border-left:3px solid @ACCENT@; }
QFrame#toast { background:@TEXTC@; color:@SURFACE@; border-left:3px solid @ACCENT@; border-radius:8px; }
QFrame#toast QLabel { color:@SURFACE@; font-weight:650; font-size:12px; }

/* --------------------------------------------------------- resume studio */
QSplitter#resumeWorkspaceSplitter::handle { background:@BORDER@; }
QSplitter#resumeWorkspaceSplitter::handle:horizontal { width:1px; margin:0; }
QSplitter#resumeWorkspaceSplitter::handle:vertical { height:1px; margin:0; }
QListWidget#resumeSectionNav { background:transparent; border:none; outline:none; padding:0; }
QListWidget#resumeSectionNav::item { background:transparent; color:@SIDEBARTEXT@;
                                      border:1px solid transparent; border-radius:7px;
                                      padding:8px 10px; font-size:12px; font-weight:600; }
QListWidget#resumeSectionNav::item:hover { background:@HOVER@; color:@TEXT@; }
QListWidget#resumeSectionNav::item:selected { background:@ACCENTSOFT@; color:@ACCENTTEXT@;
                                               border-color:@ACCENT@; font-weight:700; }
QLabel#resumeSectionTitle { color:@TEXT@; font-size:16px; font-weight:750; letter-spacing:-0.2px; }
QLabel#resumePageTitle { color:@TEXT@; font-size:19px; font-weight:800; letter-spacing:-0.3px; }
QTextBrowser#resumePreview { background:#FFFFFF; color:#1A1A1A; border:1px solid @BORDER@;
                              border-radius:2px; padding:0; }
QFrame#resumePreviewCanvas { background:@SURFACEALT@; border:1px solid @BORDER@; border-radius:9px; }
QFrame#resumeNavigationPanel, QWidget#resumeNavigationPanel { background:@SURFACE@; }
QLabel#resumeStatus { color:@MUTED@; font-size:12px; }
QLabel#resumeStatus[role="error"] { color:@DANGER@; background:@DANGERSOFT@;
                                     border:1px solid @DANGERLINE@; border-radius:7px; padding:7px 10px; }
QLabel#resumeScoreHint { color:@MUTED@; font-size:11px; }

/* ------------------------------------------------------------ auth screen */
QScrollArea#landingScroll { background:@LBG@; border:none; }
QWidget#landingPage { background:@LBG@; color:@LFG@; }
QWidget#landingHeader { background:@LBG@; }
QWidget#landingPage[compact="true"] QLabel#landingHeroTitle { font-size:32px; }
QWidget#landingPage[compact="true"] QLabel#landingSectionTitle { font-size:19px; }
QWidget#landingPage[compact="true"] QLabel#landingBrand { font-size:16px; }
QWidget#landingPage[compact="true"] QPushButton#landingPrimary,
QWidget#landingPage[compact="true"] QPushButton#landingSecondary,
QWidget#landingPage[compact="true"] QPushButton#landingGetStarted,
QWidget#landingPage[compact="true"] QPushButton#landingLogin { padding:6px 8px; font-size:11px; }
QLabel#landingBrand { color:@LFG@; font-size:18px; font-weight:800; letter-spacing:-0.3px; }
QLabel#landingTagline { color:@LMUTED@; font-size:9px; font-weight:800; letter-spacing:1.1px; }
QPushButton#landingNavLink { background:transparent; color:@LMUTED@; border:none; border-radius:7px;
                              padding:8px 11px; font-size:12px; font-weight:600; }
QPushButton#landingNavLink:hover { background:@LHOVER@; color:@LFG@; }
QPushButton#landingPrimary, QPushButton#landingGetStarted {
    background:@ACCENT@; color:#FFFFFF; border:1px solid @ACCENT@; border-radius:7px;
    padding:10px 16px; font-weight:700; }
QPushButton#landingPrimary:hover, QPushButton#landingGetStarted:hover { background:@ACCENTDARK@; }
QPushButton#landingSecondary, QPushButton#landingLogin {
    background:transparent; color:@LFG@; border:1px solid @LBORDER@; border-radius:7px;
    padding:9px 15px; font-weight:650; }
QPushButton#landingSecondary:hover, QPushButton#landingLogin:hover { background:@LHOVER@; }
QLabel#landingEyebrow { color:@ACCENT@; font-size:10px; font-weight:800; letter-spacing:1.3px; }
QLabel#landingHeroTitle { color:@LFG@; font-size:44px; font-weight:800; letter-spacing:-1.4px; }
QLabel#landingHeroText { color:@LMUTED@; font-size:14px; }
QLabel#landingTrustLine { color:@LMUTED@; font-size:11px; }
QFrame#landingEcosystem { background:@LSURFACE@; border:1px solid @LBORDER@; border-radius:11px; }
QLabel#ecosystemTitle { color:@LFG@; font-size:14px; font-weight:750; }
QLabel#ecosystemBadge { background:@LHOVER@; color:@LMUTED@; border:1px solid @LBORDER@;
                        border-radius:6px; padding:4px 8px; font-size:9px; font-weight:800;
                        letter-spacing:0.6px; }
QFrame#ecosystemNode { background:@LBG@; border:1px solid @LBORDER@; border-radius:8px; }
QLabel#ecosystemNodeNumber { color:@ACCENT@; font-size:10px; font-weight:800; }
QLabel#ecosystemNodeName { color:@LFG@; font-size:11px; font-weight:650; }
QFrame#ecosystemDivider { background:@LBORDER@; min-height:1px; max-height:1px; border:none; }
QLabel#ecosystemFootLabel { color:@LMUTED@; font-size:10px; font-weight:650; }
QLabel#ecosystemFootValue { color:@LFG@; font-size:23px; font-weight:800; letter-spacing:-0.6px; }
QLabel#landingSectionTitle { color:@LFG@; font-size:22px; font-weight:800; letter-spacing:-0.6px; }
QLabel#landingSectionText { color:@LMUTED@; font-size:12px; }
QFrame#landingMetric, QFrame[landingCard="metric"] { background:@LSURFACE@;
    border:1px solid @LBORDER@; border-radius:9px; }
QFrame#landingMetric:hover, QFrame[landingCard="metric"]:hover { border-color:@ACCENT@; }
QLabel#landingMetricValue { color:@LFG@; font-size:27px; font-weight:800; letter-spacing:-0.8px; }
QLabel#landingMetricTitle { color:@LFG@; font-size:13px; font-weight:750; }
QLabel#landingMetricText { color:@LMUTED@; font-size:11px; }
QFrame#landingJourneyPanel { background:@LSURFACE@; border:1px solid @LBORDER@; border-radius:11px; }
QLabel#landingJourneyTitle { color:@LFG@; font-size:19px; font-weight:800; letter-spacing:-0.4px; }
QLabel#landingJourneyText { color:@LMUTED@; font-size:12px; }
QFrame#landingBenefitCard { background:@LSURFACE@; border:1px solid @LBORDER@; border-radius:9px; }
QLabel#landingBenefitTitle { color:@LFG@; font-size:13px; font-weight:750; }
QLabel#landingBenefitText { color:@LMUTED@; font-size:11px; }
QWidget#landingFooter { border-top:1px solid @LBORDER@; }
QLabel#landingFooterText { color:@LMUTED@; font-size:11px; }
QFrame#showcase { background:@LBG@; }
QLabel#showTitle { color:@LFG@; font-size:34px; font-weight:800; letter-spacing:-1px; }
QLabel#showAccentTitle { color:@ACCENT@; font-size:34px; font-weight:800; letter-spacing:-1px; }
QLabel#showSub { color:@LMUTED@; font-size:13px; }
QFrame#showMetric { background:@LSURFACE@; border:1px solid @LBORDER@; border-radius:9px; }
QFrame#showMetric:hover { border-color:@ACCENT@; }
QLabel#showPill { background:@LHOVER@; border:1px solid @LBORDER@; color:@LMUTED@;
                  border-radius:6px; padding:5px 12px; font-size:10px; font-weight:800;
                  letter-spacing:1px; }
QLabel#showStat { color:@LFG@; font-size:22px; font-weight:800; letter-spacing:-0.5px; }
QLabel#showStatLabel { color:@LMUTED@; font-size:10px; font-weight:700; letter-spacing:0.5px; }
QLabel#showPrivacy { color:@LMUTED@; font-size:12px; }
QLabel#showBrand { color:@LFG@; font-size:22px; font-weight:800; letter-spacing:-0.4px; }
QWidget#compactBrandBar { background:@LBG@; border-bottom:1px solid @LBORDER@; }
QLabel#compactBrandName { color:@LFG@; font-size:17px; font-weight:800; letter-spacing:-0.3px; }
QLabel#compactBrandTagline { color:@LMUTED@; font-size:11px; }
QPushButton#authBackLink { background:transparent; color:@ACCENT@; border:none; padding:2px 0;
                            font-weight:650; text-align:left; }
QPushButton#authBackLink:hover { color:@ACCENTDARK@; }
)QSS";

    q += pillRule("success", "@SUCCESSSOFT@", "@SUCCESS@", "@SUCCESSLINE@");
    q += pillRule("info",    "@ACCENTSOFT@",  "@ACCENTTEXT@", "@BORDER@");
    q += pillRule("warning", "@WARNSOFT@",    "@WARNING@", "@WARNLINE@");
    q += pillRule("danger",  "@DANGERSOFT@",  "@DANGER@",  "@DANGERLINE@");
    q += pillRule("neutral", "@SURFACEALT@",  "@MUTED@",   "@BORDER@");
    q += pillRule("amber",   "@WARNSOFT@",    "@WARNING@", "@WARNLINE@");
    q += R"QSS(
QLabel[pillSize="small"] { padding:1px 5px; font-size:10px; border-radius:5px; }
)QSS";

    // The auth screen keeps its own darker ground in light mode so signing in
    // reads as a distinct place; in dark mode it simply follows the app.
    const QString landingBg      = dark ? c.background : QStringLiteral("#0C1512");
    const QString landingSurface = dark ? c.surface    : QStringLiteral("#15211D");
    const QString landingBorder  = dark ? c.border     : QStringLiteral("#26352F");
    const QString landingFg      = dark ? c.text       : QStringLiteral("#ECF2EF");
    const QString landingMuted   = dark ? c.textMuted  : QStringLiteral("#92A6A0");

    const QHash<QString, QString> map {
        { "@TEXT@", c.text }, { "@TEXTC@", mix(c.text, "#000000", dark ? 0.0 : 0.05) },
        { "@MUTED@", c.textMuted }, { "@BG@", c.background },
        { "@SURFACE@", c.surface }, { "@SURFACEALT@", c.surfaceAlt },
        { "@BORDER@", c.border }, { "@ACCENT@", c.accent },
        { "@ACCENTDARK@", c.primaryDark }, { "@ACCENTSOFT@", c.accentSoft },
        { "@ACCENTTEXT@", c.accentText }, { "@SIDEBAR@", c.sidebar },
        { "@SIDEBARTEXT@", c.sidebarText }, { "@HOVER@", hover },
        { "@HOVERHARD@", hoverHard }, { "@ROWHOVER@", rowHover },
        { "@SHADOW@", shadowFg },
        { "@SUCCESS@", c.success }, { "@SUCCESSSOFT@", mix(c.surface, c.success, dark ? 0.16 : 0.10) },
        { "@SUCCESSLINE@", mix(c.border, c.success, 0.35) },
        { "@WARNING@", c.warning }, { "@WARNSOFT@", mix(c.surface, c.warning, dark ? 0.16 : 0.12) },
        { "@WARNLINE@", mix(c.border, c.warning, 0.35) },
        { "@DANGER@", c.danger }, { "@DANGERSOFT@", mix(c.surface, c.danger, dark ? 0.16 : 0.08) },
        { "@DANGERLINE@", mix(c.border, c.danger, 0.35) },
        { "@LBG@", landingBg }, { "@LSURFACE@", landingSurface },
        { "@LBORDER@", landingBorder }, { "@LFG@", landingFg },
        { "@LMUTED@", landingMuted }, { "@LHOVER@", mix(landingBg, landingFg, 0.08) }
    };
    for (auto it = map.cbegin(); it != map.cend(); ++it) q.replace(it.key(), it.value());
    return q;
}

inline void applyPalette(QApplication &app) {
    const Tokens color = tokens();
    QPalette palette = app.palette();
    palette.setColor(QPalette::Window, QColor(color.background));
    palette.setColor(QPalette::Base, QColor(color.surface));
    palette.setColor(QPalette::AlternateBase, QColor(color.background));
    palette.setColor(QPalette::Button, QColor(color.surface));
    palette.setColor(QPalette::Mid, QColor(color.border));
    palette.setColor(QPalette::WindowText, QColor(color.text));
    palette.setColor(QPalette::Text, QColor(color.text));
    palette.setColor(QPalette::ButtonText, QColor(color.text));
    palette.setColor(QPalette::PlaceholderText, QColor(color.textMuted));
    palette.setColor(QPalette::Highlight, QColor(color.primary));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(palette);
}

inline void install(QApplication &app) {
    invalidateTokens();
    loadBundledFonts();
    app.setStyle("Fusion");               // consistent rendering for style sheets
    QFont f(sans());
    const QString size = QSettings().value("theme/fontSize", "Medium").toString();
    f.setPixelSize(size == "Small" ? 11 : size == "Large" ? 15 : 13);
    app.setFont(f);
    applyPalette(app);
    app.setStyleSheet(stylesheet());
}

inline void refresh(QApplication &app) {
    invalidateTokens();
    const QString size = QSettings().value("theme/fontSize", "Medium").toString();
    QFont f(sans());
    f.setPixelSize(size == "Small" ? 11 : size == "Large" ? 15 : 13);
    app.setFont(f);
    applyPalette(app);
    app.setStyleSheet(stylesheet());
}

inline QIcon appIcon() {
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(tokens().accent));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(2, 2, 60, 60, 14, 14);
    painter.setPen(QColor("#FFFFFF"));
    QFont font(sans(), 32, QFont::ExtraBold);
    painter.setFont(font);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, "P");
    return QIcon(pixmap);
}

// ---------------------------------------------------------------- helpers
inline QLabel *label(const QString &text, const char *role = nullptr, QWidget *parent = nullptr,
                     bool wrap = false) {
    auto *l = new QLabel(text, parent);
    l->setTextFormat(Qt::PlainText);
    if (role) l->setProperty("role", QString::fromLatin1(role));
    l->setWordWrap(wrap);
    return l;
}

inline QLabel *pill(const QString &text, const QString &tone) {
    auto *l = new QLabel(text);
    l->setTextFormat(Qt::PlainText);
    l->setProperty("pill", tone);
    l->setAlignment(Qt::AlignCenter);
    l->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    return l;
}

// Maps any status text ("Offer Extended", "Under Review" ...) to a pill colour.
inline QString toneFor(const QString &status) {
    const QString s = status.toLower();
    if (s.contains("not selected") || s.contains("reject") || s.contains("closed")) return "danger";
    if (s.contains("offer") || s.contains("selected") || s.contains("confirmed") || s.contains("placed")) return "success";
    if (s.contains("completed") || s.contains("registered") || s == "applied") return "neutral";
    if (s.contains("open") || s.contains("assessment")) return "info";
    return "warning";   // interview, scheduled, shortlisted, review, awaiting, test ...
}

inline QLabel *statusPill(const QString &status) { return pill(status, toneFor(status)); }

// Painted directly: no style sheet parsing per instance, so long lists stay fast.
class LogoTile : public QWidget {
    QString text;
    bool accented;
    int sz;

public:
    LogoTile(const QString &t, bool useAccent, int size)
        : text(t), accented(useAccent), sz(size) { setFixedSize(size, size); }

protected:
    void paintEvent(QPaintEvent *) override {
        // Colours read from the tokens at paint time, so avatars follow the theme
        // instead of staying on the old forest/amber pair.
        const Tokens c = tokens();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(accented ? c.accent : c.accentSoft));
        p.drawRoundedRect(rect(), sz / 3.2, sz / 3.2);
        QFont f(sans());
        f.setPixelSize(qMax(9, int(sz / 2.6)));
        f.setWeight(QFont::ExtraBold);
        f.setLetterSpacing(QFont::AbsoluteSpacing, -0.3);
        p.setFont(f);
        p.setPen(QColor(accented ? QStringLiteral("#FFFFFF") : c.accentText));
        p.drawText(rect(), Qt::AlignCenter, text);
    }
};

inline QWidget *logo(const QString &initials, bool amber = false, int size = 44) {
    return new LogoTile(initials, amber, size);
}

inline QPushButton *button(const QString &text, const char *kind = "primary") {
    auto *b = new QPushButton(text);
    b->setProperty("kind", QString::fromLatin1(kind));
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::TabFocus);
    const QString k = QString::fromLatin1(kind);
    if (k == "chip" || k == "nav" || k == "role") b->setCheckable(true);
    return b;
}

inline void shadow(QWidget *w, int blur = 30, int dy = 8, int alpha = 30) {
    auto *fx = new QGraphicsDropShadowEffect(w);
    fx->setBlurRadius(blur);
    fx->setOffset(0, dy);
    fx->setColor(QColor(60, 45, 30, alpha));
    w->setGraphicsEffect(fx);
}

// A flat surface panel: 1px border, 9px radius, and a single 1px shadow line
// under the bottom edge. The old card stacked four translucent rounded rects to
// fake a drop shadow, which read as a soft blur and dated the whole UI.
// (Painted rather than styled so long scrolling lists stay fast.)
class CardFrame : public QFrame {
    int radius;

public:
    explicit CardFrame(int r = 9, QWidget *parent = nullptr) : QFrame(parent), radius(r) {}

protected:
    void paintEvent(QPaintEvent *) override {
        const Tokens c = tokens();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -1.5);
        QColor edge(c.text);
        edge.setAlpha(isDark() ? 46 : 12);
        p.setPen(Qt::NoPen);
        p.setBrush(edge);
        p.drawRoundedRect(r.translated(0, 1), radius, radius);
        p.setBrush(QColor(c.surface));
        p.setPen(QPen(QColor(c.border), 1));
        p.drawRoundedRect(r, radius, radius);
    }
};

// Page header. The old "hero" was a dark gradient slab with amber trim on top of
// every page, which ate the first 170px of vertical space and competed with the
// content for attention. This is a plain typographic header with a rule under
// it: title, one line of context, actions on the right.
class HeroFrame : public QFrame {
    QBoxLayout *contentLayout = nullptr;
    QBoxLayout *actionLayout = nullptr;

public:
    explicit HeroFrame(int = 0, QWidget *parent = nullptr) : QFrame(parent) {
        setObjectName("pageHead");
        setAttribute(Qt::WA_StyledBackground, true);
    }

    void setResponsiveLayouts(QBoxLayout *content, QBoxLayout *actions) {
        contentLayout = content;
        actionLayout = actions;
        updateResponsiveLayout();
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QFrame::resizeEvent(event);
        updateResponsiveLayout();
    }

private:
    void updateResponsiveLayout() {
        const bool compact = width() < 680;
        if (property("compact").toBool() != compact) {
            setProperty("compact", compact);
            style()->unpolish(this);
            style()->polish(this);
        }
        if (contentLayout) {
            contentLayout->setDirection(compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
            contentLayout->setContentsMargins(0, 2, 0, compact ? 14 : 16);
            contentLayout->setSpacing(compact ? 12 : 20);
        }
        if (actionLayout) {
            actionLayout->setDirection(compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
            actionLayout->setSpacing(8);
        }
    }
};

inline QFrame *card(bool = true) { return new CardFrame(9); }

inline QFrame *divider() {
    auto *f = new QFrame;
    f->setObjectName("divider");
    f->setFixedHeight(1);
    return f;
}

inline QVBoxLayout *vbox(QWidget *w, int margin = 16, int spacing = 10) {
    auto *l = new QVBoxLayout(w);
    l->setContentsMargins(margin, margin - 2, margin, margin - 2);
    l->setSpacing(spacing);
    return l;
}

inline QHBoxLayout *hbox(QWidget *w, int margin = 0, int spacing = 12) {
    auto *l = new QHBoxLayout(w);
    l->setContentsMargins(margin, margin, margin, margin);
    l->setSpacing(spacing);
    return l;
}

inline void clearLayout(QLayout *l) {
    while (QLayoutItem *it = l->takeAt(0)) {
        if (QWidget *w = it->widget()) { w->hide(); w->deleteLater(); }
        else if (QLayout *cl = it->layout()) clearLayout(cl);
        delete it;
    }
}

// Left-aligned wrapper (used for buttons / pills inside table cells)
inline QWidget *cell(QWidget *w, int margin = 8) {
    auto *c = new QWidget;
    auto *h = new QHBoxLayout(c);
    h->setContentsMargins(margin, 0, margin, 0);
    h->addWidget(w);
    h->addStretch();
    return c;
}

inline QWidget *cellRow(const QList<QWidget *> &ws, int margin = 8) {
    auto *c = new QWidget;
    auto *h = new QHBoxLayout(c);
    h->setContentsMargins(margin, 0, margin, 0);
    h->setSpacing(8);
    for (QWidget *w : ws) h->addWidget(w);
    h->addStretch();
    return c;
}

class ResponsiveSectionHeader : public QWidget {
    QBoxLayout *box;

public:
    explicit ResponsiveSectionHeader(QWidget *parent = nullptr)
        : QWidget(parent), box(new QBoxLayout(QBoxLayout::LeftToRight, this)) {
        box->setContentsMargins(0, 0, 0, 0);
        box->setSpacing(10);
    }

    QBoxLayout *boxLayout() const { return box; }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        box->setDirection(width() < 500 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    }
};

inline QWidget *sectionHead(const QString &title, const QString &sub = QString(),
                            QWidget *trailing = nullptr) {
    auto *w = new ResponsiveSectionHeader;
    auto *h = w->boxLayout();
    auto *v = new QVBoxLayout;
    v->setSpacing(2);
    v->addWidget(label(title, "h2"));
    if (!sub.isEmpty()) v->addWidget(label(sub, "muted", nullptr, true));
    h->addLayout(v, 1);
    if (trailing) h->addWidget(trailing, 0, Qt::AlignTop);
    return w;
}

inline QFrame *emptyState(const QString &title, const QString &detail) {
    auto *frame = new QFrame;
    frame->setObjectName("emptyState");
    frame->setMinimumHeight(112);
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(5);
    layout->addStretch();
    auto *heading = label(title, "h3");
    heading->setObjectName("emptyStateTitle");
    heading->setAlignment(Qt::AlignCenter);
    auto *description = label(detail, "muted", nullptr, true);
    description->setObjectName("emptyStateDetail");
    description->setAlignment(Qt::AlignCenter);
    layout->addWidget(heading);
    layout->addWidget(description);
    layout->addStretch();
    return frame;
}

inline QFrame *hero(const QString &tag, const QString &title, const QString &sub,
                    const QList<QPushButton *> &actions = {}) {
    auto *f = new HeroFrame(0);
    auto *h = new QBoxLayout(QBoxLayout::LeftToRight, f);

    auto *left = new QVBoxLayout;
    left->setSpacing(3);
    auto *t = new QLabel(tag.toUpper());
    t->setObjectName("headTag");
    t->setTextFormat(Qt::PlainText);
    auto *ti = new QLabel(title);
    ti->setObjectName("headTitle");
    ti->setTextFormat(Qt::PlainText);
    ti->setWordWrap(true);
    auto *su = new QLabel(sub);
    su->setObjectName("headSub");
    su->setTextFormat(Qt::PlainText);
    su->setWordWrap(true);
    left->addWidget(t, 0, Qt::AlignLeft);
    left->addWidget(ti);
    if (!sub.isEmpty()) left->addWidget(su);
    else su->hide();
    h->addLayout(left, 1);

    if (!actions.isEmpty()) {
        auto *a = new QBoxLayout(QBoxLayout::LeftToRight);
        for (QPushButton *b : actions) a->addWidget(b);
        h->addLayout(a, 0);
        h->setAlignment(a, Qt::AlignTop);
        f->setResponsiveLayouts(h, a);
    } else {
        f->setResponsiveLayouts(h, nullptr);
    }
    return f;
}

inline QFrame *statCard(const QString &title, const QString &value, const QString &note) {
    auto *c = card();
    auto *v = vbox(c, 20, 4);
    v->addWidget(label(title.toUpper(), "eyebrow"));
    v->addWidget(label(value, "statValue"));
    v->addWidget(label(note, "muted", nullptr, true));
    return c;
}

// ----------------------------------------------------------- ring (donut)
// A single percentage, drawn from the theme tokens. Used for placement rate and
// profile completeness, where a bar carries less at a glance than a dial.
class RingChart : public QWidget {
    int percent;
    QString caption;
    QString tone;

public:
    explicit RingChart(int value, QString centreCaption = {}, QString toneName = QStringLiteral("accent"),
                       int diameter = 136, QWidget *parent = nullptr)
        : QWidget(parent), percent(qBound(0, value, 100)), caption(std::move(centreCaption)),
          tone(std::move(toneName)) {
        setFixedSize(diameter, diameter);
        setAccessibleName(caption.isEmpty() ? QStringLiteral("Progress ring") : caption);
        setAccessibleDescription(QString("%1 percent").arg(percent));
    }

protected:
    void paintEvent(QPaintEvent *) override {
        const Tokens colour = tokens();
        const QColor track(colour.border);
        const QColor fill(tone == "forest" ? colour.primary
                        : tone == "success" ? colour.success
                        : tone == "danger" ? colour.danger : colour.accent);
        const qreal thickness = width() / 9.0;
        const QRectF arc = QRectF(rect()).adjusted(thickness / 2 + 1, thickness / 2 + 1,
                                                   -thickness / 2 - 1, -thickness / 2 - 1);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(track, thickness, Qt::SolidLine, Qt::FlatCap));
        p.drawArc(arc, 0, 360 * 16);
        p.setPen(QPen(fill, thickness, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(arc, 90 * 16, -int(360 * 16 * percent / 100.0));

        QFont big(sans());
        big.setPixelSize(int(width() / 3.6));
        big.setLetterSpacing(QFont::AbsoluteSpacing, -1.0);
        big.setWeight(QFont::ExtraBold);
        p.setFont(big);
        p.setPen(QColor(colour.text));
        const QRect centre = rect().adjusted(0, caption.isEmpty() ? 0 : -10, 0, 0);
        p.drawText(centre, Qt::AlignCenter, QString::number(percent) + "%");
        if (caption.isEmpty()) return;
        QFont small(sans());
        small.setPixelSize(qMax(9, int(width() / 13.0)));
        small.setBold(true);
        p.setFont(small);
        p.setPen(QColor(colour.textMuted));
        p.drawText(rect().adjusted(8, height() / 2 + 14, -8, 0),
                   Qt::AlignHCenter | Qt::AlignTop, caption.toUpper());
    }
};

// Stat card with a ring instead of a bare number.
inline QFrame *ringCard(const QString &title, int percent, const QString &note,
                        const QString &centreCaption = {}, const QString &tone = "accent") {
    auto *c = card();
    auto *v = vbox(c, 20, 10);
    v->addWidget(label(title.toUpper(), "eyebrow"));
    v->addWidget(new RingChart(percent, centreCaption, tone), 0, Qt::AlignHCenter);
    v->addWidget(label(note, "muted", nullptr, true));
    return c;
}

inline QProgressBar *bar(int value, const QString &tone = "amber") {
    auto *p = new QProgressBar;
    p->setRange(0, 100);
    p->setValue(qBound(0, value, 100));
    p->setTextVisible(false);
    p->setFixedHeight(6);
    p->setProperty("tone", tone);
    return p;
}

inline QWidget *metricRow(const QString &title, const QString &note, const QString &value,
                          int pct, const QString &tone = "amber") {
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);
    auto *top = new QHBoxLayout;
    auto *left = new QVBoxLayout;
    left->setSpacing(1);
    left->addWidget(label(title, "h3"));
    if (!note.isEmpty()) left->addWidget(label(note, "muted"));
    top->addLayout(left, 1);
    auto *valueLabel = label(value, "h3");
    valueLabel->setStyleSheet("font-variant-numeric:tabular-nums;");
    top->addWidget(valueLabel, 0, Qt::AlignRight | Qt::AlignVCenter);
    v->addLayout(top);
    v->addWidget(bar(pct, tone));
    return w;
}

// ----------------------------------------------------------------- tables
inline QTableWidget *table(const QStringList &headers) {
    auto *t = new QTableWidget(0, headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->verticalHeader()->hide();
    t->verticalHeader()->setDefaultSectionSize(46);
    t->horizontalHeader()->setFixedHeight(34);
    t->horizontalHeader()->setHighlightSections(false);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    t->setShowGrid(false);
    t->setFrameShape(QFrame::NoFrame);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setFocusPolicy(Qt::NoFocus);
    t->setAlternatingRowColors(false);
    t->setWordWrap(false);
    t->setTextElideMode(Qt::ElideRight);
    t->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    t->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    t->horizontalHeader()->setMinimumSectionSize(96);
    return t;
}

// Table grows with its rows instead of scrolling inside the page.
inline void fitTable(QTableWidget *t) {
    t->setFixedHeight(34 + qMax(1, t->rowCount()) * t->verticalHeader()->defaultSectionSize() +
                      t->horizontalScrollBar()->sizeHint().height() + 2);
}

inline void setText(QTableWidget *t, int r, int c, const QString &text, bool bold = false) {
    auto *it = new QTableWidgetItem(text);
    if (bold) { QFont f = it->font(); f.setBold(true); it->setFont(f); }
    t->setItem(r, c, it);
}

inline QFrame *tableCard(const QString &title, const QString &sub, QWidget *trailing, QTableWidget *t) {
    auto *c = card();
    auto *v = new QVBoxLayout(c);
    v->setContentsMargins(0, 0, 0, 2);
    v->setSpacing(0);
    auto *hw = new QWidget;
    auto *hl = new QVBoxLayout(hw);
    hl->setContentsMargins(16, 14, 16, 10);
    hl->addWidget(sectionHead(title, sub, trailing));
    v->addWidget(hw);
    v->addWidget(t);
    return c;
}

// ---------------------------------------------------------- page scaffold
class Page : public QScrollArea {
public:
    explicit Page(QWidget *parent = nullptr) : QScrollArea(parent) {
        setWidgetResizable(true);
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
        body = new QWidget;
        body->setObjectName("pageBody");
        body->setAttribute(Qt::WA_StyledBackground, true);
        lay = new QVBoxLayout(body);
        lay->setContentsMargins(28, 22, 28, 32);
        lay->setSpacing(16);
        verticalScrollBar()->setSingleStep(28);   // smoother mouse-wheel scrolling
        setWidget(body);
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QScrollArea::resizeEvent(event);
        const int horizontal = width() < 560 ? 14 : width() < 900 ? 20 : 28;
        const int vertical = width() < 560 ? 16 : 22;
        lay->setContentsMargins(horizontal, vertical, horizontal, width() < 560 ? 24 : 32);
    }

    QWidget *body;
    QVBoxLayout *lay;
};

// Number of card columns for a given page width.
inline int columnsFor(int w) { return w >= 1180 ? 3 : (w >= 760 ? 2 : 1); }

class ResponsiveGrid : public QWidget {
    QGridLayout *grid;
    QVector<QWidget *> items;
    int minimumColumnWidth;
    int columnCount = 0;
    bool dirty = false;

public:
    explicit ResponsiveGrid(int minColumnWidth = 280, int spacing = 18, QWidget *parent = nullptr)
        : QWidget(parent), grid(new QGridLayout(this)), minimumColumnWidth(minColumnWidth) {
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(spacing);
        setMinimumWidth(0);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    void addWidget(QWidget *widget) {
        items.append(widget);
        dirty = true;
        reflow();
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        reflow();
    }

private:
    void reflow() {
        if (items.isEmpty()) return;
        const int spacing = grid->horizontalSpacing();
        const int columns = qBound(1, (width() + spacing) / (minimumColumnWidth + spacing),
                                   items.size());
        if (!dirty && columns == columnCount) return;

        const int oldRows = grid->rowCount();
        const int oldColumns = grid->columnCount();
        while (QLayoutItem *item = grid->takeAt(0)) delete item;
        for (int row = 0; row < oldRows; ++row) grid->setRowStretch(row, 0);
        for (int column = 0; column < oldColumns; ++column)
            grid->setColumnStretch(column, 0);
        columnCount = columns;
        dirty = false;
        for (int i = 0; i < items.size(); ++i) {
            const int row = i / columnCount;
            const int column = i % columnCount;
            grid->addWidget(items[i], row, column);
            grid->setColumnStretch(column, 1);
            grid->setRowStretch(row, 1);
        }
    }
};

// Small non-blocking notification at the bottom-right of the window (like the web
// toasts). Several toasts now stack upwards instead of landing on top of each
// other, slide in, and can carry a tone so failures do not look like successes.
inline void toast(QWidget *anchor, const QString &text, const QString &tone = QStringLiteral("default")) {
    QWidget *win = anchor ? anchor->window() : nullptr;
    if (!win) return;

    auto *t = new QLabel(text, win);
    t->setObjectName("toast");
    t->setProperty("tone", tone);
    t->setTextFormat(Qt::PlainText);
    t->setWordWrap(true);
    t->setMaximumWidth(qMax(220, win->width() / 2));
    t->ensurePolished();
    t->adjustSize();

    // stack above any toast already on screen
    int offset = 28;
    for (QLabel *existing : win->findChildren<QLabel *>("toast", Qt::FindDirectChildrenOnly))
        if (existing != t && existing->isVisible()) offset += existing->height() + 10;

    const int targetX = win->width() - t->width() - 28;
    const int targetY = win->height() - t->height() - offset;
    t->move(targetX + 40, targetY);
    t->show();
    t->raise();

    auto *fx = new QGraphicsOpacityEffect(t);
    fx->setOpacity(0.0);
    t->setGraphicsEffect(fx);

    auto *slide = new QPropertyAnimation(t, "pos", t);
    slide->setDuration(190);
    slide->setStartValue(QPoint(targetX + 40, targetY));
    slide->setEndValue(QPoint(targetX, targetY));
    slide->setEasingCurve(QEasingCurve::OutCubic);
    auto *fadeIn = new QPropertyAnimation(fx, "opacity", t);
    fadeIn->setDuration(190);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    slide->start(QAbstractAnimation::DeleteWhenStopped);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);

    QTimer::singleShot(2600, t, [t, fx]() {
        auto *fadeOut = new QPropertyAnimation(fx, "opacity", t);
        fadeOut->setDuration(300);
        fadeOut->setStartValue(1.0);
        fadeOut->setEndValue(0.0);
        QObject::connect(fadeOut, &QPropertyAnimation::finished, t, &QObject::deleteLater);
        fadeOut->start();
    });
}

// -------------------------------------------------------------- file export
inline QString csvLine(const QStringList &cols) {
    QStringList q;
    for (QString c : cols) {
        c.replace("\"", "\"\"");
        q << "\"" + c + "\"";
    }
    return q.join(",") + "\n";
}

inline bool saveTextFile(QWidget *parent, const QString &title, const QString &defaultName,
                         const QString &filter, const QString &content) {
    const QString path = QFileDialog::getSaveFileName(
        parent, title, QDir::homePath() + "/" + defaultName, filter);
    if (path.isEmpty()) return false;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(parent, "Export failed", "Could not write to:\n" + path);
        return false;
    }
    QTextStream out(&f);
    out << content;
    if (!f.commit()) {
        QMessageBox::critical(parent, "Export failed", "Could not safely finish writing to:\n" + path);
        return false;
    }
    QMessageBox::information(parent, "Export complete", "Saved to:\n" + path);
    return true;
}

} // namespace Theme

#endif // THEME_H
