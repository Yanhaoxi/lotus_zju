/**
 * Matrix class for FPSolve
 */

#ifndef FPSOLVE_MATRIX_H
#define FPSOLVE_MATRIX_H

#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>

namespace fpsolve {

template <typename SR>
class Matrix {
public:
  Matrix(const Matrix &m) = default;
  Matrix(Matrix &&m)
      : rows_(m.rows_), columns_(m.columns_), elements_(std::move(m.elements_)) {
    m.rows_ = 0;
    m.columns_ = 0;
  }

  Matrix(std::size_t r, const std::vector<SR> &es)
      : rows_(r), columns_(es.size() / r), elements_(es) {
    assert(Ok());
  }

  Matrix(std::size_t r, std::vector<SR> &&es)
      : rows_(r), columns_(es.size() / r), elements_(std::move(es)) {
    assert(Ok());
  }

  Matrix(std::size_t r, std::initializer_list<SR> es)
      : rows_(r), columns_(es.size() / r), elements_(es) {
    assert(Ok());
  }

  Matrix(std::size_t r, std::size_t c)
      : rows_(r), columns_(c), elements_(rows_ * columns_, SR::null()) {
    assert((r == 0 && c == 0) || (r > 0 && c > 0));
  }

  Matrix(std::size_t r, std::size_t c, const SR &elem)
      : rows_(r), columns_(c), elements_(rows_ * columns_, elem) {
    assert((r == 0 && c == 0) || (r > 0 && c > 0));
  }

  inline std::size_t GetIndex(std::size_t r, std::size_t c) const {
    return r * columns_ + c;
  }

  inline SR& At(std::size_t r, std::size_t c) {
    assert(GetIndex(r, c) < elements_.size());
    return elements_[GetIndex(r, c)];
  }

  inline const SR& At(std::size_t r, std::size_t c) const {
    assert(GetIndex(r, c) < elements_.size());
    return elements_[GetIndex(r, c)];
  }

  Matrix& operator=(const Matrix &rhs) = default;
  Matrix& operator=(Matrix &&rhs) {
    rows_ = rhs.rows_;
    rhs.rows_ = 0;
    columns_ = rhs.columns_;
    rhs.columns_ = 0;
    elements_ = std::move(rhs.elements_);
    return *this;
  }

  Matrix operator+(const Matrix &mat) const {
    assert(rows_ == mat.rows_ && columns_ == mat.columns_ &&
           elements_.size() == mat.elements_.size());
    std::vector<SR> result;
    result.reserve(elements_.size());
    for (std::size_t i = 0; i < columns_ * rows_; ++i) {
      result.emplace_back(elements_[i] + mat.elements_[i]);
    }
    return Matrix{rows_, std::move(result)};
  }

  Matrix operator*(const Matrix &rhs) const {
    assert(columns_ == rhs.rows_);
    Matrix result{rows_, rhs.columns_, SR::null()};
    for (std::size_t r = 0; r < rows_; ++r) {
      for (std::size_t c = 0; c < rhs.columns_; ++c) {
        result.At(r, c) = At(r, 0) * rhs.At(0, c);
        for (std::size_t i = 1; i < columns_; ++i) {
          result.At(r, c) += At(r, i) * rhs.At(i, c);
        }
      }
    }
    return result;
  }

  bool operator==(const Matrix &rhs) {
    assert(rows_ == rhs.rows_ && columns_ == rhs.columns_ &&
           elements_.size() == rhs.elements_.size());
    return elements_ == rhs.elements_;
  }

  // Floyd-Warshall algorithm for matrix star
  Matrix FloydWarshall() const {
    assert(columns_ == rows_);
    Matrix result = *this;
    for (std::size_t k = 0; k < rows_; ++k) {
      result.At(k,k) = result.At(k, k).star();
      for (std::size_t i = 0; i < rows_; ++i) {
        if(i==k) continue;
        result.At(i, k) = result.At(i, k) * result.At(k,k);
        for (std::size_t j = 0; j < rows_; ++j) {
          if(j==k) continue;
          result.At(i, j) += result.At(i, k) * result.At(k, j);
        }
      }
      for (std::size_t i=0; i<rows_; ++i) {
        if(i==k) continue;
        result.At(k, i) = result.At(k, k) * result.At(k,i);
      }
    }
    return result;
  }

  Matrix star() const {
    assert(columns_ == rows_);
    return recursive_star2(*this);
  }

  // Solves x = Ax + b using LDU decomposition
  Matrix solve_LDU(const Matrix& b) const;
  static Matrix subst_LDU(const Matrix& A_LDU, Matrix& rhs);
  static void LDU_decomposition_2(Matrix& A);

  std::size_t getRows() const { return rows_; }
  std::size_t getColumns() const { return columns_; }
  std::size_t getSize() const { return elements_.size(); }
  const std::vector<SR>& getElements() const { return elements_; }

  std::string string() const {
    std::stringstream ss;
    for (std::size_t r = 0; r < rows_; ++r) {
      for (std::size_t c = 0; c < columns_; ++c) {
        ss << At(r, c) << " | ";
      }
      ss << std::endl;
    }
    return ss.str();
  }

  static Matrix const null(std::size_t size) {
    return Matrix{size, size, SR::null()};
  }

  static Matrix const one(std::size_t size) {
    std::vector<SR> result;
    result.reserve(size * size);
    for (std::size_t i = 0; i < size * size; ++i) {
      if (i % (size + 1) == 0)
        result.emplace_back(SR::one());
      else
        result.emplace_back(SR::null());
    }
    return Matrix{size, std::move(result)};
  }

private:
  std::size_t rows_;
  std::size_t columns_;
  std::vector<SR> elements_;

  bool Ok() const {
    return columns_ * rows_ == elements_.size();
  }

  std::size_t Size() const { return elements_.size(); }

  // Recursive star computation (optimized version)
  static Matrix recursive_star2(Matrix matrix) {
    assert(matrix.rows_ == matrix.columns_);
    if (matrix.rows_ == 1) {
      matrix.elements_[0] = matrix.elements_[0].star();
      return matrix;
    }
    std::size_t split = matrix.columns_/2;
    Matrix a_11 = matrix.submatrix(0, split, 0, split);
    Matrix a_12 = matrix.submatrix(split, matrix.columns_, 0, split);
    Matrix a_21 = matrix.submatrix(0, split, split, matrix.rows_);
    Matrix a_22 = matrix.submatrix(split, matrix.columns_, split, matrix.rows_);

    Matrix as_11 = recursive_star2(a_11);
    Matrix a = (as_11 * a_12);
    Matrix A_22 = recursive_star2(a_22 + a_21 * a);
    Matrix A_21 = A_22 * (a_21 * as_11);
    Matrix A_12 = a * A_22;
    Matrix A_11 = a * A_21 + as_11;

    return block_matrix(std::move(A_11), std::move(A_12),
                        std::move(A_21), std::move(A_22));
  }

  static Matrix block_matrix(Matrix &&a_11, Matrix &&a_12,
                             Matrix &&a_21, Matrix &&a_22) {
    std::vector<SR> result;
    result.reserve(a_11.Size() + a_12.Size() + a_21.Size() + a_22.Size());
    assert(a_11.rows_ == a_12.rows_ && a_21.rows_ == a_22.rows_);
    assert(a_11.columns_ == a_21.columns_ && a_12.columns_ == a_22.columns_);

    for (std::size_t r = 0; r < a_11.rows_; ++r) {
      auto row_11_iter_begin = a_11.elements_.begin() + (r * a_11.columns_);
      auto row_11_iter_end = row_11_iter_begin + a_11.columns_;
      std::move(row_11_iter_begin, row_11_iter_end, std::back_inserter(result));

      auto row_12_iter_begin = a_12.elements_.begin() + (r * a_12.columns_);
      auto row_12_iter_end = row_12_iter_begin + a_12.columns_;
      std::move(row_12_iter_begin, row_12_iter_end, std::back_inserter(result));
    }
    for (std::size_t r = 0; r < a_21.rows_; ++r) {
      auto row_21_iter_begin = a_21.elements_.begin() + (r * a_21.columns_);
      auto row_21_iter_end = row_21_iter_begin + a_21.columns_;
      std::move(row_21_iter_begin, row_21_iter_end, std::back_inserter(result));

      auto row_22_iter_begin = a_22.elements_.begin() + (r * a_22.columns_);
      auto row_22_iter_end = row_22_iter_begin + a_22.columns_;
      std::move(row_22_iter_begin, row_22_iter_end, std::back_inserter(result));
    }
    return Matrix{a_11.rows_ + a_21.rows_, std::move(result)};
  }

  Matrix submatrix(std::size_t cs, std::size_t ce,
                   std::size_t rs, std::size_t re) const {
    assert(cs < columns_ && ce <= columns_ && ce > cs);
    assert(rs < rows_ && re <= rows_ && re > rs);
    std::vector<SR> result;
    result.reserve((re - rs) * (ce - cs));
    for (std::size_t r = rs; r < re; r++) {
      for (std::size_t c = cs; c < ce; c++) {
        result.emplace_back(elements_[columns_ * r + c]);
      }
    }
    return Matrix{re - rs, std::move(result)};
  }

  static void forward_substitution(const Matrix& A, Matrix& b);
  static void backward_substitution(const Matrix& A, Matrix& b);
};

template <typename SR>
std::ostream& operator<<(std::ostream& os, const Matrix<SR>& mat) {
  return os << mat.string();
}

} // namespace fpsolve

// Implementation of solve_LDU and related methods
namespace fpsolve {

template <typename SR>
Matrix<SR> Matrix<SR>::solve_LDU(const Matrix& b) const {
  assert(b.columns_ == 1 && b.rows_ == this->rows_ && this->rows_ == this->columns_);
  const std::size_t n = b.rows_;
  Matrix ldu = *this;
  Matrix rhs = b;
  LDU_decomposition_2(ldu);
  forward_substitution(ldu, rhs);
  for(std::size_t i=0; i<n; ++i) {
    rhs.At(i,0) = ldu.At(i,i) * rhs.At(i,0);
  }
  backward_substitution(ldu, rhs);
  return rhs;
}

template <typename SR>
Matrix<SR> Matrix<SR>::subst_LDU(const Matrix& A_LDU, Matrix& rhs) {
  assert(rhs.columns_ == 1 && rhs.rows_ == A_LDU.rows_ && A_LDU.rows_ == A_LDU.columns_);
  const std::size_t n = rhs.rows_;
  forward_substitution(A_LDU, rhs);
  for(std::size_t i=0; i<n; ++i) {
    rhs.At(i,0) = A_LDU.At(i,i) * rhs.At(i,0);
  }
  backward_substitution(A_LDU, rhs);
  return rhs;
}

template <typename SR>
void Matrix<SR>::LDU_decomposition_2(Matrix& A) {
  const std::size_t n = A.rows_;
  for(std::size_t k=0; k<n; ++k) {
    A.At(k,k) = A.At(k,k).star();
    for(std::size_t j=k+1; j<n; ++j) {
      A.At(k,j) = A.At(k,k) * A.At(k,j);
    }
    for(std::size_t i=k+1; i<n; ++i) {
      A.At(i,k) = A.At(i,k) * A.At(k,k);
      for(std::size_t j=k+1; j<n; ++j) {
        A.At(i,j) += A.At(i,k) * A.At(k,j);
      }
    }
  }
}

template <typename SR>
void Matrix<SR>::forward_substitution(const Matrix& A, Matrix& b) {
  const std::size_t n = A.rows_;
  for(std::size_t i=0; i<n-1; ++i) {
    for(std::size_t j=i+1; j<n; ++j) {
      b.At(j,0) += A.At(j,i) * b.At(i,0);
    }
  }
}

template <typename SR>
void Matrix<SR>::backward_substitution(const Matrix& A, Matrix& b) {
  const std::size_t n = A.rows_;
  for(std::size_t ii=1; ii<=n; ++ii) {
    std::size_t i = n - ii;
    for(std::size_t jj=0; jj<ii-1; ++jj) {
      std::size_t j = n - jj - 1;
      b.At(i,0) += A.At(i,j) * b.At(j,0);
    }
  }
}

} // namespace fpsolve

#endif // FPSOLVE_MATRIX_H

