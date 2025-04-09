#ifndef COUNTMINSKETCH_HPP
#define COUNTMINSKETCH_HPP

#include <vector>
#include <random>
#include <chrono>


//E ach element in _bits is a 32-bit int storing 8 counters (4 bits per counter)

class CountMinRow {
    private:
        friend class CountMinSketch;
        std::vector<int> _bits;

        int get(int value) const {
            int countIndex = value / 8; // get vector index
            int countOffset = value % 8 * 4; // get the offset in index with 4-bit stride

            return (_bits[countIndex] >> countOffset) & 0x7;
        }

        void increment(int value) {
            int countIndex = value / 8; // get vector index
            int countOffset = value % 8 * 4; // get the offset in index with 4-bit stride

            int value = (_bits[countIndex] >> countOffset) & 0x7;

            if (value < 15) {
                _bits[countIndex] += (1 << countOffset);
		    }
        }

        void clear() {
            for (auto& it : _bits) {
                it = 0;
            }
        }
        /*
        This line: Halves all counters by shifting right... Removes invalid bits caused by carry-over ... Keeps all 4-bit counters valid and self-contained*/
        void decay() {
            for (auto& it : _bits) {
                // 0x77777777 = 
                // 0111 0111 0111 0111 0111.
                // When we bitshift left, we have carry over digits
                // && these with 0x777777... guarantees these digits are set to 0!
                it = (it >> 1) & 0x77777777;
            }
        }

    public:
        explicit CountMinRow(size_t countNum) : _bits((countNum < 8 ? 8 : countNum) / 8, 0) {}
};    

class CountMinSketch {    
    private:
        std::vector<CountMinRow> rows_;
        std::vector<uint32_t> seed_;	
        uint32_t mask_;					
    
        static const int COUNT_MIN_SKETCH_DEPTH = 4;
    
        int next2Power(int x) {
            x--;
            x |= x >> 1;
            x |= x >> 2;
            x |= x >> 4;
            x |= x >> 8;
            x |= x >> 16;
            x |= x >> 32;
            x++;
            return x;
        }
    public:
        explicit CountMinSketch(size_t countNum) : seed_(4) {
            countNum = next2Power(countNum);
            if (countNum < 8) {
                countNum = 8;
            }
            rows_.resize(4, CountMinRow(countNum));
            mask_ = countNum - 1;
    
            unsigned time = std::chrono::system_clock::now().time_since_epoch().count();
            std::mt19937 generator(time);
            for (int i = 0; i < COUNT_MIN_SKETCH_DEPTH; i++) {
                generator.discard(generator.state_size);
                seed_[i] = generator();
            }
        }
    
        int getCountMin(uint32_t hash) {
            int min = 16, value = 0;
            for (int i = 0; i < rows_.size(); i++) {
                value = rows_[i].get((hash ^ seed_[i]) & mask_);
    
                min = (value < min) ? value : min;
            }
            return min;
        }
    
        void increment(uint32_t hash) {
            for (int i = 0; i < rows_.size(); i++) {
                rows_[i].increment((hash ^ seed_[i]) & mask_);
            }
        }
    
        void decay() {
            for (auto& row : rows_) {
                row.decay();
            }
        }
    
        void clear() {
            for (auto& row : rows_) {
                row.clear();
            }
        }
    };

#endif