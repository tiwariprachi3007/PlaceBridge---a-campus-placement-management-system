#ifndef CALENDAR_PAGE_H
#define CALENDAR_PAGE_H

#include "theme.h"
#include "context.h"
#include "dialogs.h"
#include "placement_officer.h"
#include <QMultiHash>

// Month grid built from real QDate maths + events.txt. Prev / Next move months.
class CalendarPage : public Theme::Page {
    Ctx ctx;
    bool isAdmin = false;
    QDate month;
    QLabel *monthTitle = nullptr;
    QWidget *gridHost = nullptr;
    QGridLayout *grid = nullptr;
    QWidget *agendaHost = nullptr;
    QVBoxLayout *agendaLay = nullptr;
    QWidget *upcomingHost = nullptr;
    QVBoxLayout *upcomingLay = nullptr;
    QVector<Event> allEvents;              // read once; a data change rebuilds the page
    QMultiHash<QDate, int> eventsByDate;   // date -> index into allEvents

protected:
    void resizeEvent(QResizeEvent *event) override {
        Theme::Page::resizeEvent(event);
        updateCalendarLayout();
    }

public:
    explicit CalendarPage(const Ctx &c) : ctx(c) {
        isAdmin = dynamic_cast<PlacementOfficer *>(ctx.user) != nullptr;
        month = QDate(QDate::currentDate().year(), QDate::currentDate().month(), 1);

        auto *prev = Theme::button("← Prev Month", "ghost");
        auto *next = Theme::button("Next Month →", "ghost");
        QList<QPushButton *> actions = { prev, next };
        if (isAdmin) {
            auto *add = Theme::button("+ Schedule Event", "primary");
            QObject::connect(add, &QPushButton::clicked, this, [this]() {
                EventDialog dlg(this);
                if (dlg.exec() != QDialog::Accepted) return;
                const Event event = dlg.toEvent();
                const QString conflict = ctx.store->interviewConflictReason(event);
                if (!conflict.isEmpty()) {
                    QMessageBox::warning(this, "Interview conflict", conflict);
                    return;
                }
                if (!ctx.store->addEvent(event)) {
                    QMessageBox::warning(this, "Event not saved",
                        "The event could not be saved. Check permissions and the database.");
                    return;
                }
                Theme::toast(this, "Event added to calendar");
            });
            actions.append(add);
        }
        QObject::connect(prev, &QPushButton::clicked, this, [this]() {
            month = month.addMonths(-1);
            rebuildGrid();
            rebuildAgenda();
        });
        QObject::connect(next, &QPushButton::clicked, this, [this]() {
            month = month.addMonths(1);
            rebuildGrid();
            rebuildAgenda();
        });
        // the month buttons are wired before loadEvents(); both read `allEvents`
        lay->addWidget(Theme::hero("Recruitment Event Timeline", "Recruitment Calendar",
                                   "Online technical assessments, corporate presentations, and interview panels.", actions));

        // legend + month title
        auto *head = new Theme::ResponsiveGrid(145, 8);
        monthTitle = Theme::label("", "h2");
        head->addWidget(monthTitle);
        head->addWidget(Theme::pill("Assessment", "info"));
        head->addWidget(Theme::pill("Interview", "warning"));
        head->addWidget(Theme::pill("Completed", "neutral"));
        lay->addWidget(head);

        lay->addWidget(Theme::label("Focus on upcoming rounds, interviews and assessments in one glance.",
                                    "muted", nullptr, true));

        auto *calCard = Theme::card();
        auto *cv = Theme::vbox(calCard, 16, 0);
        gridHost = new QWidget;
        grid = new QGridLayout(gridHost);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(0);
        cv->addWidget(gridHost);
        agendaHost = new QWidget;
        agendaLay = new QVBoxLayout(agendaHost);
        agendaLay->setContentsMargins(0, 0, 0, 0);
        agendaLay->setSpacing(8);
        cv->addWidget(agendaHost);
        lay->addWidget(calCard);

        // upcoming rounds
        auto *upCard = Theme::card();
        auto *uv = Theme::vbox(upCard, 22, 12);
        auto *exp = Theme::button("Export .ics", "soft");
        QObject::connect(exp, &QPushButton::clicked, this, [this]() { exportIcs(); });
        uv->addWidget(Theme::sectionHead("Upcoming Selection Rounds", "Next assessments and interviews from today.", exp));
        upcomingHost = new QWidget;
        upcomingLay = new QVBoxLayout(upcomingHost);
        upcomingLay->setContentsMargins(0, 0, 0, 0);
        upcomingLay->setSpacing(10);
        uv->addWidget(upcomingHost);
        lay->addWidget(upCard);
        lay->addStretch();

        loadEvents();
        rebuildGrid();
        rebuildAgenda();
        buildUpcoming();
        updateCalendarLayout();
    }

private:
    // The grid, the agenda and the upcoming list each used to run their own
    // events() query; they now share one read plus a date index.
    void loadEvents() {
        allEvents = ctx.store->events();
        eventsByDate.clear();
        for (int i = 0; i < allEvents.size(); ++i)
            eventsByDate.insert(allEvents[i].date, i);
    }

    void updateCalendarLayout() {
        if (!gridHost || !agendaHost) return;
        const bool compact = width() < 700;
        gridHost->setVisible(!compact);
        agendaHost->setVisible(compact);
    }

    // An event is over when its *state* says so; `type` only names the round.
    static bool isFinished(const Event &event) {
        const QString state = event.state.trimmed();
        return state.compare("Completed", Qt::CaseInsensitive) == 0 ||
               state.compare("Cancelled", Qt::CaseInsensitive) == 0;
    }

    static QString chipTone(const QString &type) {
        return type == "Assessment" ? "info" : (type == "Interview" ? "warning" : "neutral");
    }

    void rebuildGrid() {
        Theme::clearLayout(grid);
        monthTitle->setText(month.toString("MMMM yyyy"));
        const QStringList dows = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
        for (int i = 0; i < 7; ++i) {
            auto *l = Theme::label(dows[i], "dow");
            l->setAlignment(Qt::AlignCenter);
            l->setFixedHeight(28);
            grid->addWidget(l, 0, i);
            grid->setColumnStretch(i, 1);
        }
        const int offset = month.dayOfWeek() % 7;          // Sunday = 0
        const QDate start = month.addDays(-offset);
        const int rows = (offset + month.daysInMonth() + 6) / 7;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < 7; ++c) {
                const QDate d = start.addDays(r * 7 + c);
                auto *cellFrame = new QFrame;
                cellFrame->setObjectName("dayCell");
                cellFrame->setMinimumHeight(96);
                cellFrame->setProperty("today", d == QDate::currentDate());
                cellFrame->setProperty("other", d.month() != month.month());
                auto *v = new QVBoxLayout(cellFrame);
                v->setContentsMargins(6, 6, 6, 6);
                v->setSpacing(3);
                auto *num = Theme::label(QString::number(d.day()),
                                         d.month() != month.month() ? "dayNumOther"
                                         : (d == QDate::currentDate() ? "dayNumToday" : "dayNum"));
                v->addWidget(num);
                int shown = 0, extra = 0;
                const QList<int> dayEvents = eventsByDate.values(d);
                for (auto it = dayEvents.crbegin(); it != dayEvents.crend(); ++it) {
                    const Event &e = allEvents[*it];
                    if (shown == 3) { ++extra; continue; }
                    auto *chip = Theme::pill(e.title, chipTone(e.type));
                    chip->setToolTip(e.time + " · " + e.title + "\n" + e.detail);
                    chip->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);   // elide instead of widening the column
                    chip->setProperty("pillSize", "small");
                    v->addWidget(chip);
                    ++shown;
                }
                if (extra) v->addWidget(Theme::label(QString("+%1 more").arg(extra), "muted"));
                v->addStretch();
                grid->addWidget(cellFrame, r + 1, c);
            }
            grid->setRowStretch(r + 1, 1);
        }
    }

    void rebuildAgenda() {
        Theme::clearLayout(agendaLay);
        QMap<QDate, QVector<Event>> byDate;
        for (const Event &event : allEvents)
            if (event.date.year() == month.year() && event.date.month() == month.month())
                byDate[event.date].append(event);
        if (byDate.isEmpty()) {
            agendaLay->addWidget(Theme::label("No events scheduled this month.", "muted", nullptr, true));
            return;
        }
        for (auto it = byDate.cbegin(); it != byDate.cend(); ++it) {
            for (int i = 0; i < it.value().size(); ++i) {
                const Event &event = it.value()[i];
                auto *row = new QHBoxLayout;
                row->setSpacing(12);
                auto *date = Theme::label(i == 0 ? it.key().toString("ddd, dd MMM") : QString(),
                                          "h3", nullptr, true);
                date->setFixedWidth(96);
                row->addWidget(date, 0, Qt::AlignTop);
                auto *details = new QVBoxLayout;
                details->setSpacing(2);
                details->addWidget(Theme::label(event.company.isEmpty() ? event.title : event.company,
                                                "h3", nullptr, true));
                details->addWidget(Theme::label(event.title + " · " + event.time + " · " + event.detail,
                                                "muted", nullptr, true));
                row->addLayout(details, 1);
                row->addWidget(Theme::pill(event.type, chipTone(event.type)), 0, Qt::AlignTop);
                agendaLay->addLayout(row);
                if (i + 1 < it.value().size()) agendaLay->addWidget(Theme::divider());
            }
        }
    }

    void buildUpcoming() {
        Theme::clearLayout(upcomingLay);
        int n = 0;
        for (const Event &e : allEvents) {
            if (e.date < QDate::currentDate() || isFinished(e)) continue;
            if (n++ == 4) break;
            auto *row = new QHBoxLayout;
            row->setSpacing(14);
            auto *date = Theme::label(e.date.toString("dd MMM"), "h3");
            date->setFixedWidth(72);
            row->addWidget(date);
            auto *t = new QVBoxLayout;
            t->setSpacing(2);
            t->addWidget(Theme::label(e.company.isEmpty() ? e.title : e.company, "h3"));
            t->addWidget(Theme::label(e.title + " · " + e.time + " · " + e.detail, "muted", nullptr, true));
            row->addLayout(t, 1);
            row->addWidget(Theme::statusPill(e.state), 0, Qt::AlignTop);
            upcomingLay->addLayout(row);
        }
        if (n == 0) upcomingLay->addWidget(Theme::label("Nothing scheduled from today onwards.", "muted"));
    }

    void exportIcs() {
        QString ics = "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//PlaceBridge//Calendar//EN\r\n";
        for (const Event &e : allEvents) {
            const QDateTime start(e.date, QTime::fromString(e.time, "HH:mm"));
            ics += "BEGIN:VEVENT\r\n";
            ics += "UID:" + QUuid::createUuid().toString(QUuid::WithoutBraces) + "@placebridge\r\n";
            ics += "DTSTART:" + start.toString("yyyyMMdd'T'HHmmss") + "\r\n";
            ics += "DTEND:" + start.addSecs(3600).toString("yyyyMMdd'T'HHmmss") + "\r\n";
            ics += "SUMMARY:" + e.title + "\r\n";
            ics += "DESCRIPTION:" + e.detail + "\r\n";
            ics += "END:VEVENT\r\n";
        }
        ics += "END:VCALENDAR\r\n";
        Theme::saveTextFile(this, "Export calendar", "placebridge.ics", "iCalendar (*.ics)", ics);
    }
};

#endif // CALENDAR_PAGE_H
