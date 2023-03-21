#include "read_input_functions.h" 
using namespace std;

// Функция чтения строки ввода без указания количества строк
string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}


// Функция чтения строки ввода с указанием количества строк
int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}