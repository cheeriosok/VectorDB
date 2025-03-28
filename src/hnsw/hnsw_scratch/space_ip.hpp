#include "hnswlib.hpp"

template <typename Scalar>
struct InnerProduct {
    [[nodiscard]] inline Scalar operator()(const Scalar* vec1, const Scalar* vec2, size_t dim) const {
        Scalar dot_product = 0;
        for (size_t i = 0; i < dim; ++i)
            dot_product += vec1[i] * vec2[i];
        return dot_product;
    }
};

template <typename Scalar>
struct InnerProductDistance {
    [[nodiscard]] inline Scalar operator()(const Scalar* vec1, const Scalar* vec2, size_t dim) const {
        return 1.0f - InnerProduct<Scalar>{}(vec1, vec2, dim);
    }
};

template <typename Scalar, typename DistanceFunction>
class InnerProductSpace : public SpaceInterface<Scalar, DistanceFunction> {
    DistanceFunction distance_function_;
    size_t dim_;
    size_t data_size_;

public:
    InnerProductSpace(size_t dim) 
        : distance_function_(), dim_(dim), data_size_(dim * sizeof(Scalar)) {}

    size_t get_data_size() const override {
        return data_size_;
    }

    DistanceFunc get_distance_function_() const override {
        return distance_function_;
    }

    size_t get_dim() const override {
        return dim_;
    }
};
