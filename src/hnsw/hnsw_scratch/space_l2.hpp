template <typename Scalar>
struct L2Sqr {
    [[nodiscard]] inline Scalar operator()(const Scalar* vec1, const Scalar* vec2, const size_t* dim) const {
        
        Scalar result = 0;
        for (size_t i = 0; i < *dim; i++) {
            Scalar diff = *vec1 - *vec2;
            vec1++;
            vec2++;
            result += diff * diff;
        }
        return result;
    }
};

template <typename Scalar, typename DistanceFunction>
class L2Space : public SpaceInterface<Scalar, DistanceFunction> {
    DistanceFunction distance_function_;
    size_t dim_;
    size_t data_size_;

public:
   L2Space(size_t dim) 
        : distance_function_(), dim_(dim), data_size_(dim * sizeof(Scalar)) {}

    size_t get_data_size() const override {
        return data_size_;
    }

    DistanceFunction get_distance_function_() const override {
        return distance_function_;
    }

    size_t get_dim() const override {
        return dim_;
    }
};

template <typename Scalar = unsigned char>
struct L2Sqr4x {
    [[nodiscard]] inline int operator()(const Scalar* vec1, const Scalar* vec2, const size_t* dim) const {
        int result = 0;
        d >>= 2; 
        for (size_t i = 0; i < *dim; ++i) {
            result += (vec1[0] - vec2[0]) * (vec1[0] - vec2[0]);
            result += (vec1[1] - vec2[1]) * (vec1[1] - vec2[1]);
            result += (vec1[2] - vec2[2]) * (vec1[2] - vec2[2]);
            result += (vec1[3] - vec2[3]) * (vec1[3] - vec2[3]);

            vec1 += 4;
            vec2 += 4;
        }
        return result;
    }
};

template <typename Scalar = unsigned char>
struct L2SqrI{
    [[nodiscard]] inline int operator()(const Scalar* __restrict vec1, const Scalar* __restrict vec2, const size_t* __restrict dim) const {
        int res = 0;
        for (size_t i = 0; i < *dim; i++) {
            res += ((*vec1) - (*vec2)) * ((*vec1) - (*vec2));
            vec1++;
            vec2++;
        }
        return (res);
    }
};

template <typename Scalar, typename DistanceFunction>
class L2SpaceI : public SpaceInterface<Scalar, DistanceFunction> {
    DistanceFunction distance_function_;
    size_t dim_;
    size_t data_size_;

public:
    L2SpaceI(size_t dim) {
        if (dim % 4 == 0) {
            distance_function_ = L2SqrI4x{};
        } else {
            distance_function_ = L2SqrI;
        }
        dim_ = dim;
        data_size_ = dim * sizeof(unsigned char);
    }

    size_t get_data_size() const override {
        return data_size_;
    }

    DistanceFunction get_distance_function_() const override {
        return distance_function_;
    }

    void *get_distance_function_parameters_ram() {
        return &dim_;
    }
    ~L2SpaceI() {}

};

