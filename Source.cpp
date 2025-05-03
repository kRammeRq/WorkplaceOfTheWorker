#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <sstream>
#include <string>
#include <iomanip>
#include <unordered_map>
#include <algorithm>
#include <nlohmann/json.hpp>
using namespace std;
using json = nlohmann::json;
//Перевод строки в нижний регистр
string toLower(const string& s) {
    string result = s;
    transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return tolower(c); });
    return result;
}
//Стуктура продукта
struct Product {
    string name;
    string barcode;
    double price;
};

struct SaleItem {
    Product product;
    int quantity;
};

enum class PaymentType { CASH, CARD };
//Структура чека
struct Receipt {
    vector<SaleItem> items;
    PaymentType paymentType;
    double total;
    double paid;
    double change;
};
//Структура смены
struct Shift {
    string name_cashier;
    double initialCash;
    bool open = false;
    double cashTotal = 0;
    double cardTotal = 0;
};

vector<Product> productDB;
Shift currentShift;
unordered_map<string, size_t> barcodeMap;
unordered_map<string, size_t> nameMap;
//Загрузка базы данных продуктов
void loadProducts(const string& filename) {
    ifstream file(filename);
    if (!file) {
        cerr << "Ошибка: не удалось открыть файл базы товаров.\n";
        exit(1);
    }
    json j;
    file >> j;
    for (const auto& item : j) {
        string name = toLower(item["name"]);
        productDB.push_back({ name, item["barcode"], item["price"] });
        size_t idx = productDB.size() - 1;
        barcodeMap[productDB[idx].barcode] = idx;
        nameMap[productDB[idx].name] = idx;
    }
}
//Поиск продукта в базе данных
Product* findProduct(const string& inputRaw) {
    string input = toLower(inputRaw);
    auto itBarcode = barcodeMap.find(input);
    if (itBarcode != barcodeMap.end()) return &productDB[itBarcode->second];
    auto itName = nameMap.find(input);
    if (itName != nameMap.end()) return &productDB[itName->second];
    return nullptr;
}
//Печать чека
void printReceipt(const Receipt& r) {
    cout << "------------------- ЧЕК -------------------\n";
    for (const auto& item : r.items) {
        cout << item.product.name << " x" << item.quantity
            << " = " << item.quantity * item.product.price << " руб.\n";
    }
    cout << "Итого: " << r.total << " руб.\n";
    cout << "Оплата: " << (r.paymentType == PaymentType::CASH ? "Наличные" : "Картой") << "\n";
    cout << "Получено: " << r.paid << " руб.\n";
    cout << "Сдача: " << r.change << " руб.\n";
    cout << "-------------------------------------------\n";
}
//Открытие смены
void openShift() {
    if (currentShift.open) {
        cerr << "Смена уже открыта.\n";
        return;
    }
    cout << "Введите имя кассира: ";
    getline(cin, currentShift.name_cashier);
    cout << "Введите начальную сумму в кассе: ";
    string choiceStr;
    while (true) {
        getline(cin, choiceStr);
        int money;
        try {
            money = stoi(choiceStr);
        }
        catch (...) {
            cerr << "Неверный ввод. Введите число.\n";
            continue;
        }
        currentShift.initialCash = money;
        break;
    }
    currentShift.open = true;
    currentShift.cashTotal = 0;
    currentShift.cardTotal = 0;
    cout << "Смена открыта.\n";
}
//Закрытие смены
void closeShift() {
    if (!currentShift.open) {
        cerr << "Смена не открыта.\n";
        return;
    }
    cout << "---------- Отчет о закрытии смены ----------\n";
    cout << "Кассир: " << currentShift.name_cashier << "\n";
    cout << "Сумма наличных при закрытии смены: " << currentShift.initialCash << " руб.\n";
    cout << "Продажи наличными: " << currentShift.cashTotal << " руб.\n";
    cout << "Продажи по карте: " << currentShift.cardTotal << " руб.\n";
    cout << "Итого выручка: " << (currentShift.cashTotal + currentShift.cardTotal) << " руб.\n";
    cout << "--------------------------------------------\n";
    currentShift = Shift(); // сброс
}
//Формирование нового чека
void newReceipt() {
    if (!currentShift.open) {
        cerr << "Смена не открыта.\n";
        return;
    }
    Receipt r;
    r.total = 0;
    while (true) {
        cout << "Введите штрих-код или название товара (или 'enter', чтобы завершить ввод товаров): ";
        string input;
        getline(cin, input);
        if (input == "enter") break;
        Product* p = findProduct(input);
        if (!p) {
            cerr << "Товар не найден.\n";
            continue;
        }
        cout << "Введите количество: ";
        string qty_str;
        getline(cin, qty_str);  
        int qty;
        try {
            qty = stoi(qty_str);  
        }
        catch (...) {
            cerr << "Ошибка: некорректное количество.\n";
            continue;
        }
        r.items.push_back({ *p, qty });
        r.total += p->price * qty;
    }
    cout << "Итого к оплате: " << r.total << " руб.\n";
    cout << "Способ оплаты (1 - наличные, 2 - карта): ";
    int payment;
    cin >> payment;
    cin.ignore();
    r.paymentType = (payment == 1 ? PaymentType::CASH : PaymentType::CARD);
    if (r.paymentType == PaymentType::CASH) {
        cout << "Введите сумму, полученную от покупателя: ";
        cin >> r.paid;
        cin.ignore();
        if (r.paid < r.total) {
            cerr << "Недостаточно средств.\n";
            return;
        }
        r.change = r.paid - r.total;
        // Проверка: хватает ли наличности в кассе на сдачу
        if (currentShift.initialCash < r.change) {
            cerr << "Недостаточно наличных в кассе для сдачи. Операция отменена.\n";
            return;
        }
        currentShift.initialCash += r.paid;  // добавляем выручку
        currentShift.initialCash -= r.change; // выдаём сдачу
        currentShift.cashTotal += r.total;
    }
    else {
        r.paid = r.total;
        r.change = 0;
        currentShift.cardTotal += r.total;
    }
    printReceipt(r);
}

int main() {
    loadProducts("products.json");
    while (true) {
        cout << "\n1. Открыть смену\n2. Новый чек\n3. Закрыть смену\n4. Выход\nВыберите действие: ";
        string choiceStr;
        getline(cin, choiceStr);
        int choice;
        try {
            choice = stoi(choiceStr);
        }
        catch (...) {
            cerr << "Неверный ввод. Введите число.\n";
            continue;
        }
        switch (choice) {
        case 1: openShift(); break;
        case 2: newReceipt(); break;
        case 3: closeShift(); break;
        case 4: return 0;
        default: cerr << "Неверный выбор.\n";
        }
    }
    return 0;
}
