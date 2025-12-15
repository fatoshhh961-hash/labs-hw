#include <iostream>
#include <fstream>
#include <cstdint>
#include <array>
#include <vector>
#include <chrono>

using std::cout;
using std::cin;
using std::endl;
using std::ifstream;
using std::ofstream;
using std::ios;

// константы для вычисления интервалов
static const uint32_t RANGE_MAX = 0xFFFF;
static const uint32_t RANGE_MID = (RANGE_MAX + 1) / 2;
static const uint32_t RANGE_Q1 = RANGE_MID / 2;
static const uint32_t RANGE_Q3 = RANGE_Q1 * 3;

// класс для записи битов
class OutputBitStream {
private:
    ofstream& output_stream;
    uint8_t bit_buffer = 0;
    int bits_in_buffer = 0;

public:
    OutputBitStream(ofstream& os) : output_stream(os) {}

    void write_bit(int bit_value) {
        bit_buffer = (bit_buffer << 1) | (bit_value & 1);
        bits_in_buffer++;

        if (bits_in_buffer == 8) {
            output_stream.put(static_cast<char>(bit_buffer));
            bit_buffer = 0;
            bits_in_buffer = 0;
        }
    }

    void write_repeated(int bit_value, uint32_t repeat_count) {
        for (uint32_t i = 0; i < repeat_count; i++) {
            write_bit(bit_value);
        }
    }

    void finalize() {
        if (bits_in_buffer > 0) {
            bit_buffer <<= (8 - bits_in_buffer);
            output_stream.put(static_cast<char>(bit_buffer));
            bits_in_buffer = 0;
            bit_buffer = 0;
        }
    }
};

// класс для чтения битов
class InputBitStream {
private:
    ifstream& input_stream;
    uint8_t bit_buffer = 0;
    int bits_remaining = 0;

public:
    InputBitStream(ifstream& is) : input_stream(is) {}

    int read_bit() {
        if (bits_remaining == 0) {
            int byte_read = input_stream.get();
            if (byte_read == EOF) return -1;
            bit_buffer = static_cast<uint8_t>(byte_read);
            bits_remaining = 8;
        }
        int current_bit = (bit_buffer >> (bits_remaining - 1)) & 1;
        bits_remaining--;
        return current_bit;
    }

    int read_bits(int bit_count) {
        int value = 0;
        for (int i = 0; i < bit_count; i++) {
            int bit = read_bit();
            if (bit == -1) return -1;
            value = (value << 1) | bit;
        }
        return value;
    }
};

// функция кодирования (сжатия)
bool compress_data(double& compression_ratio) {
    ifstream source_file("text.txt", ios::binary);
    if (!source_file) {
        cout << "Ошибка: файл text.txt не найден" << endl;
        return false;
    }

    std::array<uint32_t, 256> char_counts = {};
    uint64_t char_total = 0;
    int current_char;

    while ((current_char = source_file.get()) != EOF) {
        char_counts[static_cast<unsigned char>(current_char)]++;
        char_total++;
    }

    ofstream output_file("encoded.txt", ios::binary);
    if (!output_file) {
        cout << "Ошибка: не удалось создать encoded.txt" << endl;
        return false;
    }

    // запись заголовка
    output_file.write(reinterpret_cast<char*>(&char_total), sizeof(char_total));
    output_file.write(reinterpret_cast<char*>(char_counts.data()),
        sizeof(uint32_t) * 256);

    if (char_total == 0) {
        compression_ratio = 1.0;
        return true;
    }

    // вычисление кумулятивных частот
    std::vector<uint32_t> cumulative(257, 0);
    for (int i = 0; i < 256; i++) {
        cumulative[i + 1] = cumulative[i] + char_counts[i];
    }
    uint32_t frequency_total = cumulative[256];

    source_file.clear();
    source_file.seekg(0, ios::beg);

    OutputBitStream bit_writer(output_file);

    uint32_t interval_low = 0;
    uint32_t interval_high = RANGE_MAX;
    uint32_t pending_counter = 0;

    while ((current_char = source_file.get()) != EOF) {
        unsigned char symbol = static_cast<unsigned char>(current_char);
        uint32_t current_range = interval_high - interval_low + 1;

        uint32_t new_low = interval_low +
            static_cast<uint64_t>(current_range) * cumulative[symbol] / frequency_total;
        uint32_t new_high = interval_low +
            static_cast<uint64_t>(current_range) * cumulative[symbol + 1] / frequency_total - 1;

        interval_low = new_low;
        interval_high = new_high;

        while (true) {
            if (interval_high < RANGE_MID) {
                bit_writer.write_bit(0);
                bit_writer.write_repeated(1, pending_counter);
                pending_counter = 0;
            }
            else if (interval_low >= RANGE_MID) {
                bit_writer.write_bit(1);
                bit_writer.write_repeated(0, pending_counter);
                pending_counter = 0;
                interval_low -= RANGE_MID;
                interval_high -= RANGE_MID;
            }
            else if (interval_low >= RANGE_Q1 && interval_high < RANGE_Q3) {
                pending_counter++;
                interval_low -= RANGE_Q1;
                interval_high -= RANGE_Q1;
            }
            else {
                break;
            }

            interval_low = (interval_low << 1) & RANGE_MAX;
            interval_high = ((interval_high << 1) | 1) & RANGE_MAX;
        }
    }

    pending_counter++;
    if (interval_low < RANGE_Q1) {
        bit_writer.write_bit(0);
        bit_writer.write_repeated(1, pending_counter);
    }
    else {
        bit_writer.write_bit(1);
        bit_writer.write_repeated(0, pending_counter);
    }

    bit_writer.finalize();

    source_file.seekg(0, ios::end);
    output_file.seekp(0, ios::end);
    double input_size = source_file.tellg();
    double output_size = output_file.tellp();
    compression_ratio = output_size / input_size;

    return true;
}

// функция декодирования (распаковки)
bool decompress_data() {
    ifstream input_file("encoded.txt", ios::binary);
    if (!input_file) {
        cout << "Ошибка: файл encoded.txt не найден" << endl;
        return false;
    }

    uint64_t char_total = 0;
    std::array<uint32_t, 256> char_counts = {};

    input_file.read(reinterpret_cast<char*>(&char_total), sizeof(char_total));
    input_file.read(reinterpret_cast<char*>(char_counts.data()),
        sizeof(uint32_t) * 256);

    std::vector<uint32_t> cumulative(257, 0);
    for (int i = 0; i < 256; i++) {
        cumulative[i + 1] = cumulative[i] + char_counts[i];
    }
    uint32_t frequency_total = cumulative[256];

    ofstream output_file("decoded.txt", ios::binary);
    if (!output_file) {
        cout << "Ошибка: не удалось создать decoded.txt" << endl;
        return false;
    }

    InputBitStream bit_reader(input_file);
    int initial_bits = bit_reader.read_bits(16);
    if (initial_bits < 0) initial_bits = 0;

    uint32_t current_value = initial_bits;
    uint32_t interval_low = 0;
    uint32_t interval_high = RANGE_MAX;

    for (uint64_t i = 0; i < char_total; i++) {
        uint32_t current_range = interval_high - interval_low + 1;
        uint32_t scaled_value = static_cast<uint64_t>(current_value - interval_low + 1) *
            frequency_total - 1;
        scaled_value /= current_range;

        int left_index = 0;
        int right_index = 256;

        while (right_index - left_index > 1) {
            int middle = (left_index + right_index) / 2;
            if (cumulative[middle] <= scaled_value) {
                left_index = middle;
            }
            else {
                right_index = middle;
            }
        }

        int symbol_index = left_index;
        output_file.put(static_cast<char>(symbol_index));

        uint32_t new_low = interval_low +
            static_cast<uint64_t>(current_range) * cumulative[symbol_index] / frequency_total;
        uint32_t new_high = interval_low +
            static_cast<uint64_t>(current_range) * cumulative[symbol_index + 1] / frequency_total - 1;

        interval_low = new_low;
        interval_high = new_high;

        while (true) {
            if (interval_high < RANGE_MID) {
                // Ничего не делаем
            }
            else if (interval_low >= RANGE_MID) {
                current_value -= RANGE_MID;
                interval_low -= RANGE_MID;
                interval_high -= RANGE_MID;
            }
            else if (interval_low >= RANGE_Q1 && interval_high < RANGE_Q3) {
                current_value -= RANGE_Q1;
                interval_low -= RANGE_Q1;
                interval_high -= RANGE_Q1;
            }
            else {
                break;
            }

            interval_low = (interval_low << 1) & RANGE_MAX;
            interval_high = ((interval_high << 1) | 1) & RANGE_MAX;

            int next_bit = bit_reader.read_bit();
            if (next_bit < 0) next_bit = 0;
            current_value = ((current_value << 1) | next_bit) & RANGE_MAX;
        }
    }

    return true;
}

// основное меню программы
int main() {
    int user_selection;
    setlocale(LC_ALL, "Russian");
    cout << "Программа арифметического сжатия данных" << endl;
    cout << "=======================================" << endl;
    cout << "1 - Сжать файл text.txt (создать encoded.txt)" << endl;
    cout << "2 - Распаковать файл encoded.txt (создать decoded.txt)" << endl;
    cout << "Выберите действие: ";

    cin >> user_selection;

    if (user_selection == 1) {
        auto time_start = std::chrono::high_resolution_clock::now();

        double compression_result = 0.0;
        if (!compress_data(compression_result)) {
            return 1;
        }

        auto time_end = std::chrono::high_resolution_clock::now();
        double elapsed_time = std::chrono::duration<double>(time_end - time_start).count();

        cout << "Сжатие успешно завершено." << endl;
        cout << "Коэффициент сжатия: " << compression_result << endl;
        cout << "Время выполнения: " << elapsed_time << " секунд" << endl;
    }
    else if (user_selection == 2) {
        auto time_start = std::chrono::high_resolution_clock::now();

        if (!decompress_data()) {
            return 1;
        }

        auto time_end = std::chrono::high_resolution_clock::now();
        double elapsed_time = std::chrono::duration<double>(time_end - time_start).count();

        cout << "Распаковка успешно завершена." << endl;
        cout << "Время выполнения: " << elapsed_time << " секунд" << endl;
    }
    else {
        cout << "Ошибка: неверный выбор" << endl;
    }

    return 0;
}