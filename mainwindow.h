#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlDatabase>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:

    void on_addButton_clicked();

    void on_editButton_clicked();

    void on_deleteButton_clicked();

    void on_viewButton_clicked();

private:
    Ui::MainWindow *ui;
    bool createTables();
    void viewBooks();
    void deleteBook();
    void editBook();
    void addBook();
};

#endif // MAINWINDOW_H
