#include <iostream>
#include <vector>
#include <map>
#include <list>
#include <fstream>

using namespace std;

vector<bool> code;
map<char, vector<bool>> codeMap;

class TreeNode {
public:
    int freq;
    char ch;
    TreeNode* l, * r;

    TreeNode() { l = r = NULL; }
    TreeNode(TreeNode* L, TreeNode* R) {
        l = L; r = R;
        freq = L->freq + R->freq;
    }
};

struct CompareNodes {
    bool operator()(TreeNode* a, TreeNode* b) const {
        return a->freq < b->freq;
    }
};

void GenerateCodes(TreeNode* node) {
    if (node->l != NULL) {
        code.push_back(0);
        GenerateCodes(node->l);
        code.pop_back();
    }

    if (node->r != NULL) {
        code.push_back(1);
        GenerateCodes(node->r);
        code.pop_back();
    }

    if (node->l == NULL && node->r == NULL) {
        codeMap[node->ch] = code;
    }
}

void WriteFreqTable(const map<char, int>& freqMap, ofstream& outFile) {
    int size = freqMap.size();
    outFile.write((char*)&size, sizeof(int));

    for (auto& item : freqMap) {
        outFile.write(&item.first, 1);
        outFile.write((char*)&item.second, sizeof(int));
    }
}

map<char, int> ReadFreqTable(ifstream& inFile) {
    map<char, int> freqMap;
    int size;

    inFile.read((char*)&size, sizeof(int));

    for (int i = 0; i < size; i++) {
        char ch;
        int freq;
        inFile.read(&ch, 1);
        inFile.read((char*)&freq, sizeof(int));
        freqMap[ch] = freq;
    }

    return freqMap;
}

void EncodeFile(const map<char, int>& freqMap) {
    ofstream encoded("encoded.txt", ios::binary);

    // записываем таблицу частот
    WriteFreqTable(freqMap, encoded);

    ifstream input("text.txt", ios::binary);

    char buffer = 0;
    int pos = 0;

    while (true) {
        char c = input.get();
        if (input.eof()) break;

        vector<bool> bits = codeMap[c];

        for (bool bit : bits) {
            buffer |= (bit << (7 - pos));
            pos++;

            if (pos == 8) {
                encoded.write(&buffer, 1);
                pos = 0;
                buffer = 0;
            }
        }
    }

    // дозаписываем последние биты
    if (pos > 0) {
        buffer <<= (8 - pos);
        encoded.write(&buffer, 1);
    }

    input.close();
    encoded.close();
}

void DecodeFile() {
    ifstream encoded("encoded.txt", ios::binary);

    if (!encoded) {
        cout << "Не найден файл encoded.txt\n";
        return;
    }

    // 1. читаем таблицу частот
    map<char, int> freqMap = ReadFreqTable(encoded);

    // 2. строим дерево хаффмана
    list<TreeNode*> nodes;

    for (auto& item : freqMap) {
        TreeNode* node = new TreeNode;
        node->ch = item.first;
        node->freq = item.second;
        nodes.push_back(node);
    }

    while (nodes.size() > 1) {
        nodes.sort(CompareNodes());
        TreeNode* left = nodes.front(); nodes.pop_front();
        TreeNode* right = nodes.front(); nodes.pop_front();
        TreeNode* parent = new TreeNode(left, right);
        nodes.push_back(parent);
    }

    TreeNode* root = nodes.front();

    // 3. генерируем коды
    codeMap.clear();
    GenerateCodes(root);

    // 4. декодируем
    ofstream decoded("decoded.txt", ios::binary);

    TreeNode* current = root;
    int bitPos;
    char byte;

    while (encoded.get(byte)) {
        for (bitPos = 7; bitPos >= 0; bitPos--) {
            bool bit = byte & (1 << bitPos);

            if (bit) current = current->r;
            else current = current->l;

            // дошли до листа
            if (current->l == NULL && current->r == NULL) {
                decoded.put(current->ch);
                current = root;
            }
        }
    }

    decoded.close();
    encoded.close();
}

int main() {
    int choice;
    setlocale(LC_ALL, "Russian");
    cout << "Выберите действие:\n";
    cout << "1 - Закодировать файл (text.txt)\n";
    cout << "2 - Раскодировать файл (encoded.txt)\n";
    cout << "> ";
    cin >> choice;

    if (choice == 1) {
        // считаем частоты символов
        ifstream file("text.txt", ios::binary);
        map<char, int> freqMap;

        while (!file.eof()) {
            char c = file.get();
            if (file.eof()) break;
            freqMap[c]++;
        }
        file.close();

        // строим дерево
        list<TreeNode*> nodes;

        for (auto& item : freqMap) {
            TreeNode* node = new TreeNode;
            node->ch = item.first;
            node->freq = item.second;
            nodes.push_back(node);
        }

        while (nodes.size() != 1) {
            nodes.sort(CompareNodes());
            TreeNode* left = nodes.front(); nodes.pop_front();
            TreeNode* right = nodes.front(); nodes.pop_front();
            TreeNode* parent = new TreeNode(left, right);
            nodes.push_back(parent);
        }

        TreeNode* root = nodes.front();

        // генерируем коды
        GenerateCodes(root);

        // кодируем
        EncodeFile(freqMap);

        cout << "Кодирование завершено -> encoded.txt\n";
    }
    else if (choice == 2) {
        DecodeFile();
        cout << "Декодирование завершено -> decoded.txt\n";
    }
    else {
        cout << "Неверный выбор\n";
    }

    return 0;
}