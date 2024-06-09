#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QInputDialog>
#include <QTableWidgetItem>
#include <QSqlQuery>
#include <QMessageBox>
#include <QSqlError>
#include <QDate>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL");
    qDebug() << db.drivers();
    db.setDatabaseName("postgres");
    db.setPort(5432);
    db.setUserName("postgres");
    db.setPassword("");

    if (!db.open())
    {
        QMessageBox::warning(0, "Ошибка БД", db.lastError().databaseText());
    }
    else
    {
        QMessageBox::information(0, "Успешно", "Соединение с БД Установлено");
        createTables();

        // Обнуление последовательности для record_id
        QSqlQuery query(db);
        if (!query.exec(
                "DO $$"
                "DECLARE"
                "    max_record_id INTEGER;"
                "BEGIN"
                "    SELECT MAX(record_id) INTO max_record_id FROM borrow_records;"
                "    IF max_record_id IS NOT NULL THEN"
                "        PERFORM setval(pg_get_serial_sequence('borrow_records', 'record_id'), max_record_id + 1);"
                "    END IF;"
                "END $$;"
                )) {
            QMessageBox::warning(0, "Ошибка", query.lastError().databaseText());
        }
    }
}


MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::createTables()
{
    QSqlQuery query(QSqlDatabase::database("QPSQL"));

    QStringList tableCreationQueries = {
        // Создаем таблицу authors
        "CREATE TABLE IF NOT EXISTS authors("
        "author_id SERIAL PRIMARY KEY,"
        "name VARCHAR NOT NULL)",

        // Создаем таблицу books
        "CREATE TABLE IF NOT EXISTS books("
        "book_id SERIAL PRIMARY KEY,"
        "author_id INT NOT NULL,"
        "title VARCHAR NOT NULL,"
        "publication_year INT,"
        "FOREIGN KEY (author_id) REFERENCES authors(author_id))",

        // Создаем таблицу borrowers
        "CREATE TABLE IF NOT EXISTS borrowers("
        "borrower_id SERIAL PRIMARY KEY,"
        "name VARCHAR NOT NULL,"
        "contact_info VARCHAR)",

        // Создаем таблицу borrow_records
        "CREATE TABLE IF NOT EXISTS borrow_records("
        "record_id SERIAL PRIMARY KEY,"
        "book_id INT NOT NULL,"
        "borrower_id INT NOT NULL,"
        "borrow_date DATE,"
        "return_date DATE,"
        "FOREIGN KEY (book_id) REFERENCES books(book_id),"
        "FOREIGN KEY (borrower_id) REFERENCES borrowers(borrower_id))"
    };

    foreach (const QString &tableQuery, tableCreationQueries) {
        if (!query.exec(tableQuery)) {
            QMessageBox::warning(0, "Ошибка", query.lastError().databaseText());
            qDebug() << query.lastError().databaseText();
            return false;
        }
    }

    return true;
}

void MainWindow::viewBooks()
{
    QSqlQuery query(QSqlDatabase::database("postgres"));
    if (!query.exec("SELECT br.record_id, b.title, bo.name, bo.contact_info, br.borrow_date, br.return_date "
                    "FROM borrow_records br "
                    "JOIN books b ON br.book_id = b.book_id "
                    "JOIN borrowers bo ON br.borrower_id = bo.borrower_id"))
    {
        QMessageBox::warning(0, "Ошибка", query.lastError().databaseText());
        return;
    }

    // Устанавливаем заголовки таблицы
    ui->tableWidget->setColumnCount(6); // Обновляем количество столбцов
    QStringList headers;
    headers << "Запись ID" << "Название Книги" << "Имя Заёмщика" << "Контактная информация" << "Дата выдачи" << "Дата возврата";
    ui->tableWidget->setHorizontalHeaderLabels(headers);

    // Заполняем таблицу данными
    ui->tableWidget->setRowCount(0);  // Очистить таблицу перед заполнением
    int row = 0;
    while (query.next()) {
        ui->tableWidget->insertRow(row);
        ui->tableWidget->setItem(row, 0, new QTableWidgetItem(query.value("record_id").toString()));
        ui->tableWidget->setItem(row, 1, new QTableWidgetItem(query.value("title").toString()));
        ui->tableWidget->setItem(row, 2, new QTableWidgetItem(query.value("name").toString()));
        ui->tableWidget->setItem(row, 3, new QTableWidgetItem(query.value("contact_info").toString())); // Новый столбец
        ui->tableWidget->setItem(row, 4, new QTableWidgetItem(query.value("borrow_date").toString()));
        ui->tableWidget->setItem(row, 5, new QTableWidgetItem(query.value("return_date").toString()));
        row++;
    }
}



void MainWindow::deleteBook()
{
    QList<QTableWidgetItem *> selectedItems = ui->tableWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Выберите запись для удаления");
        return;
    }

    int row = selectedItems.first()->row();
    int record_id = ui->tableWidget->item(row, 0)->text().toInt();

    // Получаем номер записи для подтверждения
    QString record_number = ui->tableWidget->item(row, 0)->text();

    // Показываем диалоговое окно для подтверждения удаления
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Подтверждение удаления",
                                  QString("Вы действительно хотите удалить запись с номером %1?").arg(record_number),
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QSqlQuery query(QSqlDatabase::database("postgres"));
        query.prepare("DELETE FROM borrow_records WHERE record_id = :record_id");
        query.bindValue(":record_id", record_id);

        if (!query.exec()) {
            QMessageBox::warning(0, "Ошибка", query.lastError().databaseText());
        } else {
            QMessageBox::information(0, "Успешно", "Запись успешно удалена.");
            ui->tableWidget->removeRow(row); // Удалить строку из таблицы
        }
    } else {
        QMessageBox::information(0, "Отмена", "Удаление записи отменено.");
    }
}

void MainWindow::editBook()
{
    // Создание запроса для получения всех записей с названиями книг и именами заемщиков
    QSqlQuery query(QSqlDatabase::database("postgres"));
    if (!query.exec("SELECT br.record_id, b.title, bo.name, bo.contact_info, br.borrow_date, br.return_date "
                    "FROM borrow_records br "
                    "JOIN books b ON br.book_id = b.book_id "
                    "JOIN borrowers bo ON br.borrower_id = bo.borrower_id")) {
        QMessageBox::warning(0, "Ошибка", query.lastError().databaseText());
        return;
    }

    // Создание списка записей для выбора
    QStringList recordList;
    while (query.next()) {
        QString record = QString("Запись ID: %1, Книга: %2, Заемщик: %3, Контактная информация: %4, Дата выдачи: %5, Дата возврата: %6")
                             .arg(query.value("record_id").toString())
                             .arg(query.value("title").toString())
                             .arg(query.value("name").toString())
                             .arg(query.value("contact_info").toString())
                             .arg(query.value("borrow_date").toString())
                             .arg(query.value("return_date").toString());
        recordList << record;
    }

    // Диалог для выбора записи
    bool ok;
    QString selectedRecord = QInputDialog::getItem(this, tr("Редактирование записи"), tr("Выберите запись для редактирования:"), recordList, 0, false, &ok);
    if (!ok || selectedRecord.isEmpty()) {
        return; // Если не выбрана запись, выходим из функции
    }

    // Получение ID выбранной записи
    QStringList selectedRecordDetails = selectedRecord.split(", ");
    int selectedRecordId = selectedRecordDetails[0].split(" ").last().toInt();

    // Получение новых данных
    QString newBorrowDate = QInputDialog::getText(this, tr("Редактирование записи"), tr("Введите новую дату выдачи (формат YYYY-MM-DD):"), QLineEdit::Normal, QString(), &ok);
    if (!ok || newBorrowDate.isEmpty()) return;

    QString newReturnDate = QInputDialog::getText(this, tr("Редактирование записи"), tr("Введите новую дату возврата (формат YYYY-MM-DD):"), QLineEdit::Normal, QString(), &ok);
    if (!ok || newReturnDate.isEmpty()) return;

    QDate borrowDate = QDate::fromString(newBorrowDate, "yyyy-MM-dd");
    QDate returnDate = QDate::fromString(newReturnDate, "yyyy-MM-dd");
    if (borrowDate.isNull() || returnDate.isNull() || returnDate < borrowDate) {
        QMessageBox::warning(0, "Ошибка", "Дата возврата не может быть раньше даты выдачи.");
        return;
    }

    // Проверка, если вводится новое имя заемщика и контактная информация
    QString newBorrowerName = QInputDialog::getText(this, tr("Редактирование записи"), tr("Введите новое имя заемщика (оставьте пустым, если не изменяется):"), QLineEdit::Normal, QString(), &ok);
    if (!ok) return;

    QString newContactInfo;
    if (!newBorrowerName.isEmpty()) {
        newContactInfo = QInputDialog::getText(this, tr("Редактирование записи"), tr("Введите новую контактную информацию заемщика:"), QLineEdit::Normal, QString(), &ok);
        if (!ok || newContactInfo.isEmpty()) return;
    }

    QSqlQuery updateQuery(QSqlDatabase::database("postgres"));
    if (!newBorrowerName.isEmpty()) {
        // Добавление нового заемщика в базу данных
        QSqlQuery borrowerQuery(QSqlDatabase::database("postgres"));
        borrowerQuery.prepare("INSERT INTO borrowers (name, contact_info) VALUES (:name, :contact_info) RETURNING borrower_id");
        borrowerQuery.bindValue(":name", newBorrowerName);
        borrowerQuery.bindValue(":contact_info", newContactInfo);
        if (!borrowerQuery.exec() || !borrowerQuery.next()) {
            QMessageBox::warning(0, "Ошибка", borrowerQuery.lastError().databaseText());
            return;
        }
        int newBorrowerId = borrowerQuery.value(0).toInt();

        // Обновление записи с новым заемщиком
        updateQuery.prepare("UPDATE borrow_records SET borrow_date = :borrow_date, return_date = :return_date, borrower_id = :borrower_id WHERE record_id = :record_id");
        updateQuery.bindValue(":borrow_date", newBorrowDate);
        updateQuery.bindValue(":return_date", newReturnDate);
        updateQuery.bindValue(":borrower_id", newBorrowerId);
        updateQuery.bindValue(":record_id", selectedRecordId);
    } else {
        // Обновление записи без изменения заемщика
        updateQuery.prepare("UPDATE borrow_records SET borrow_date = :borrow_date, return_date = :return_date WHERE record_id = :record_id");
        updateQuery.bindValue(":borrow_date", newBorrowDate);
        updateQuery.bindValue(":return_date", newReturnDate);
        updateQuery.bindValue(":record_id", selectedRecordId);
    }

    if (!updateQuery.exec()) {
        QMessageBox::warning(0, "Ошибка", updateQuery.lastError().databaseText());
    } else {
        QMessageBox::information(0, "Успешно", "Запись успешно обновлена.");
        viewBooks(); // Обновить таблицу
    }
}



void MainWindow::addBook()
{
    bool ok;

    // Получение максимального значения record_id из таблицы borrow_records
    int maxRecordId = 0;
    QSqlQuery maxIdQuery(QSqlDatabase::database("postgres"));
    if (maxIdQuery.exec("SELECT MAX(record_id) FROM borrow_records")) {
        if (maxIdQuery.next()) {
            maxRecordId = maxIdQuery.value(0).toInt();
        }
    } else {
        QMessageBox::warning(0, "Ошибка", maxIdQuery.lastError().databaseText());
        return;
    }

    // Начальное значение для новой записи
    int record_id = maxRecordId + 1;

    // Запрос для получения доступных книг
    QSqlQuery bookQuery(QSqlDatabase::database("postgres"));
    if (!bookQuery.exec("SELECT book_id, title FROM books")) {
        QMessageBox::warning(0, "Ошибка", bookQuery.lastError().databaseText());
        return;
    }
    QStringList bookList;
    while (bookQuery.next()) {
        bookList << QString("%1: %2").arg(bookQuery.value("book_id").toString()).arg(bookQuery.value("title").toString());
    }
    QString selectedBook = QInputDialog::getItem(this, tr("Добавление записи"), tr("Выберите книгу:"), bookList, 0, false, &ok);
    if (!ok || selectedBook.isEmpty()) {
        return;
    }
    int book_id = selectedBook.split(": ").first().toInt();

    // Получение имени нового заемщика
    QString borrowerName = QInputDialog::getText(this, tr("Добавление записи"), tr("Введите имя заемщика:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || borrowerName.isEmpty()) {
        return;
    }

    // Получение контактной информации нового заемщика
    QString contactInfo = QInputDialog::getText(this, tr("Добавление записи"), tr("Введите контактную информацию заемщика:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || contactInfo.isEmpty()) {
        return;
    }

    // Получение даты выдачи и возврата
    QString borrow_date = QInputDialog::getText(this, tr("Добавление записи"), tr("Введите дату выдачи (формат YYYY-MM-DD):"), QLineEdit::Normal, QString(), &ok);
    if (!ok || borrow_date.isEmpty()) {
        return;
    }
    QString return_date = QInputDialog::getText(this, tr("Добавление записи"), tr("Введите дату возврата (формат YYYY-MM-DD):"), QLineEdit::Normal, QString(), &ok);
    if (!ok || return_date.isEmpty()) {
        return;
    }

    // Проверка корректности дат
    QDate borrowDate = QDate::fromString(borrow_date, "yyyy-MM-dd");
    QDate returnDate = QDate::fromString(return_date, "yyyy-MM-dd");
    if (borrowDate.isNull() || returnDate.isNull() || returnDate < borrowDate) {
        QMessageBox::warning(0, "Ошибка", "Дата возврата не может быть раньше даты выдачи.");
        return;
    }

    // Добавление заемщика в базу данных
    QSqlQuery borrowerQuery(QSqlDatabase::database("postgres"));
    borrowerQuery.prepare("INSERT INTO borrowers (name, contact_info) VALUES (:name, :contact_info) RETURNING borrower_id");
    borrowerQuery.bindValue(":name", borrowerName);
    borrowerQuery.bindValue(":contact_info", contactInfo);
    if (!borrowerQuery.exec() || !borrowerQuery.next()) {
        QMessageBox::warning(0, "Ошибка", borrowerQuery.lastError().databaseText());
        return;
    }
    int borrower_id = borrowerQuery.value(0).toInt();

    // Добавление записи в базу данных
    QSqlQuery query(QSqlDatabase::database("postgres"));
    query.prepare("INSERT INTO borrow_records (record_id, book_id, borrower_id, borrow_date, return_date) VALUES (:record_id, :book_id, :borrower_id, :borrow_date, :return_date)");
    query.bindValue(":record_id", record_id);
    query.bindValue(":book_id", book_id);
    query.bindValue(":borrower_id", borrower_id);
    query.bindValue(":borrow_date", borrow_date);
    query.bindValue(":return_date", return_date);
    if (!query.exec()) {
        QMessageBox::warning(0, "Ошибка", query.lastError().databaseText());
    } else {
        QMessageBox::information(0, "Успешно", "Запись успешно добавлена.");
        viewBooks(); // Обновить таблицу
    }
}







void MainWindow::on_addButton_clicked()
{
    addBook();
}


void MainWindow::on_editButton_clicked()
{
    editBook();
}


void MainWindow::on_deleteButton_clicked()
{
    deleteBook();
}


void MainWindow::on_viewButton_clicked()
{
    viewBooks();
}
