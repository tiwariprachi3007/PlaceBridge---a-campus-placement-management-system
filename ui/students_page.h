#ifndef STUDENTS_PAGE_H
#define STUDENTS_PAGE_H

#include "context.h"
#include "theme.h"

class StudentsPage : public Theme::Page {
    Q_OBJECT
    Ctx ctx;
    QString branch = "All Branches";
    QLineEdit *search = nullptr;
    QComboBox *pageSize = nullptr;
    QTableWidget *table = nullptr;
    QFrame *emptyState = nullptr;
    QLabel *pageLabel = nullptr;
    QPushButton *previous = nullptr;
    QPushButton *next = nullptr;
    int currentPage = 0;
    int sortColumn = 1;
    Qt::SortOrder sortOrder = Qt::AscendingOrder;
    QVector<Student> displayed;

public:
    explicit StudentsPage(const Ctx &context);

private:
    void refresh();
    void removeSelected();
    void shortlistSelected();
    void exportSelected();
    void importCsv();
    QVector<Student> selectedStudents() const;
};

#endif // STUDENTS_PAGE_H
