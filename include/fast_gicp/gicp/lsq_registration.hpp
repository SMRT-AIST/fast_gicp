#ifndef FAST_GICP_LSQ_REGISTRATION_HPP
#define FAST_GICP_LSQ_REGISTRATION_HPP

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/registration/registration.h>

namespace fast_gicp {

enum class LSQ_OPTIMIZER_TYPE { GaussNewton, LevenbergMarquardt };

template <int Dim>
struct Foo {
  virtual Eigen::Matrix<double, Dim, Dim> reduce_H(const Eigen::Matrix<double, 6, 6>& H_in) const { return H_in; }
  virtual Eigen::Matrix<double, Dim, 1> reduce_b(const Eigen::Matrix<double, 6, 1>& b_in) const { return b_in; }
  virtual Eigen::Matrix<double, 6, 1> expand_b(const Eigen::Matrix<double, Dim, 1>& b_in) const { return b_in; }
  using Ptr = std::shared_ptr<Foo<Dim>>;
};

struct TranslationOnly : public Foo<3> {
  Eigen::Matrix<double, 3, 3> reduce_H(const Eigen::Matrix<double, 6, 6>& H_in) const override { return H_in.bottomRightCorner(3, 3); }
  Eigen::Matrix<double, 3, 1> reduce_b(const Eigen::Matrix<double, 6, 1>& b_in) const override { return b_in.bottomRightCorner(3, 1); }
  Eigen::Matrix<double, 6, 1> expand_b(const Eigen::Matrix<double, 3, 1>& b_in) const override {
    Eigen::Matrix<double, 6, 1> d = Eigen::Matrix<double, 6, 1>::Zero();
    d.bottomRightCorner(3, 1) = b_in;
    return d;
  }
};

template <bool B, bool... Args>
struct count_true {
  static constexpr int value = count_true<B>::value + count_true<Args...>::value;
};

template <>
struct count_true<true> {
  static constexpr int value = 1;
};
template <>
struct count_true<false> {
  static constexpr int value = 0;
};

template <bool... Args>
struct CustomDOF : public Foo<count_true<Args...>::value> {
  using KeepDOF = std::array<bool, count_true<Args...>::value>;
  KeepDOF keep_dof;
  CustomDOF(const KeepDOF& keep) {}
};

template <typename PointSource, typename PointTarget, int Dim = 6>
class LsqRegistration : public pcl::Registration<PointSource, PointTarget, float> {
public:
  using Scalar = float;
  using Matrix4 = typename pcl::Registration<PointSource, PointTarget, Scalar>::Matrix4;

  using PointCloudSource = typename pcl::Registration<PointSource, PointTarget, Scalar>::PointCloudSource;
  using PointCloudSourcePtr = typename PointCloudSource::Ptr;
  using PointCloudSourceConstPtr = typename PointCloudSource::ConstPtr;

  using PointCloudTarget = typename pcl::Registration<PointSource, PointTarget, Scalar>::PointCloudTarget;
  using PointCloudTargetPtr = typename PointCloudTarget::Ptr;
  using PointCloudTargetConstPtr = typename PointCloudTarget::ConstPtr;

#if PCL_VERSION >= PCL_VERSION_CALC(1, 10, 0)
  using Ptr = pcl::shared_ptr<LsqRegistration<PointSource, PointTarget>>;
  using ConstPtr = pcl::shared_ptr<const LsqRegistration<PointSource, PointTarget>>;
#else
  using Ptr = boost::shared_ptr<LsqRegistration<PointSource, PointTarget>>;
  using ConstPtr = boost::shared_ptr<const LsqRegistration<PointSource, PointTarget>>;
#endif

protected:
  using pcl::Registration<PointSource, PointTarget, Scalar>::input_;
  using pcl::Registration<PointSource, PointTarget, Scalar>::nr_iterations_;
  using pcl::Registration<PointSource, PointTarget, Scalar>::max_iterations_;
  using pcl::Registration<PointSource, PointTarget, Scalar>::final_transformation_;
  using pcl::Registration<PointSource, PointTarget, Scalar>::transformation_epsilon_;
  using pcl::Registration<PointSource, PointTarget, Scalar>::converged_;

public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  LsqRegistration();
  virtual ~LsqRegistration();

  void setRotationEpsilon(double eps);
  void setInitialLambdaFactor(double init_lambda_factor);
  void setDebugPrint(bool lm_debug_print);

  void setFoo(const typename Foo<Dim>::Ptr foo);

  const Eigen::Matrix<double, 6, 6>& getFinalHessian() const;

  double evaluateCost(const Eigen::Matrix4f& relative_pose, Eigen::Matrix<double, 6, 6>* H = nullptr, Eigen::Matrix<double, 6, 1>* b = nullptr);

  virtual void swapSourceAndTarget() {}
  virtual void clearSource() {}
  virtual void clearTarget() {}

protected:
  virtual void computeTransformation(PointCloudSource& output, const Matrix4& guess) override;

  bool is_converged(const Eigen::Isometry3d& delta) const;

  virtual double linearize(const Eigen::Isometry3d& trans, Eigen::Matrix<double, 6, 6>* H = nullptr, Eigen::Matrix<double, 6, 1>* b = nullptr) = 0;
  virtual double compute_error(const Eigen::Isometry3d& trans) = 0;

  bool step_optimize(Eigen::Isometry3d& x0, Eigen::Isometry3d& delta);
  bool step_gn(Eigen::Isometry3d& x0, Eigen::Isometry3d& delta);
  bool step_lm(Eigen::Isometry3d& x0, Eigen::Isometry3d& delta);

protected:
  double rotation_epsilon_;

  LSQ_OPTIMIZER_TYPE lsq_optimizer_type_;
  int lm_max_iterations_;
  double lm_init_lambda_factor_;
  double lm_lambda_;
  bool lm_debug_print_;

  typename Foo<Dim>::Ptr foo_;

  Eigen::Matrix<double, 6, 6> final_hessian_;
};
}  // namespace fast_gicp

#endif
