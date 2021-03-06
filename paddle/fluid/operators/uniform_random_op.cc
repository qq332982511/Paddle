/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */
#include "paddle/fluid/framework/op_registry.h"
#include "paddle/fluid/framework/operator.h"

namespace paddle {
namespace operators {

// It seems that Eigen::Tensor::random in GPU will SEGFAULT.
// Use std::random and thrust::random(thrust is a std library in CUDA) to
// implement uniform random.
template <typename T>
class CPUUniformRandomKernel : public framework::OpKernel<T> {
 public:
  void Compute(const framework::ExecutionContext& ctx) const override {
    framework::Tensor* tensor = nullptr;
    auto out_var = ctx.OutputVar("Out");
    if (out_var->IsType<framework::LoDTensor>()) {
      tensor = out_var->GetMutable<framework::LoDTensor>();
    } else if (out_var->IsType<framework::SelectedRows>()) {
      auto shape = ctx.Attr<std::vector<int>>("shape");
      tensor = out_var->GetMutable<framework::SelectedRows>()->mutable_value();
      tensor->Resize(framework::make_ddim(shape));
    } else {
      PADDLE_THROW(
          "uniform_random_op's output only"
          "supports SelectedRows and Tensor");
    }
    T* data = tensor->mutable_data<T>(ctx.GetPlace());
    unsigned int seed = static_cast<unsigned int>(ctx.Attr<int>("seed"));
    std::minstd_rand engine;
    if (seed == 0) {
      seed = std::random_device()();
    }
    engine.seed(seed);
    std::uniform_real_distribution<T> dist(
        static_cast<T>(ctx.Attr<float>("min")),
        static_cast<T>(ctx.Attr<float>("max")));
    int64_t size = tensor->numel();
    for (int64_t i = 0; i < size; ++i) {
      data[i] = dist(engine);
    }
  }
};

class UniformRandomOp : public framework::OperatorWithKernel {
 public:
  using framework::OperatorWithKernel::OperatorWithKernel;

  void InferShape(framework::InferShapeContext* ctx) const override {
    PADDLE_ENFORCE(ctx->HasOutput("Out"),
                   "Output(Out) of UniformRandomOp should not be null.");

    PADDLE_ENFORCE(
        ctx->Attrs().Get<float>("min") < ctx->Attrs().Get<float>("max"),
        "uniform_random's min must less then max");
    auto& shape = ctx->Attrs().Get<std::vector<int>>("shape");
    std::vector<int64_t> temp;
    temp.reserve(shape.size());
    for (auto dim : shape) {
      temp.push_back(static_cast<int64_t>(dim));
    }
    ctx->SetOutputDim("Out", framework::make_ddim(temp));
  }

 protected:
  framework::OpKernelType GetExpectedKernelType(
      const framework::ExecutionContext& ctx) const override {
    return framework::OpKernelType(
        static_cast<framework::proto::VarType::Type>(ctx.Attr<int>("dtype")),
        ctx.GetPlace());
  }
};

class UniformRandomOpMaker : public framework::OpProtoAndCheckerMaker {
 public:
  UniformRandomOpMaker(OpProto* proto, OpAttrChecker* op_checker)
      : framework::OpProtoAndCheckerMaker(proto, op_checker) {
    AddOutput("Out", "(Tensor) The output tensor of uniform random op");
    AddComment(R"DOC(
Uniform random operator.

This operator initializes a tensor with random values sampled from a
uniform distribution.

)DOC");
    AddAttr<std::vector<int>>("shape",
                              "(vector<int>) The shape of the output tensor");
    AddAttr<float>("min",
                   "(float, default -1.0) "
                   "Minimum value of uniform random")
        .SetDefault(-1.0f);
    AddAttr<float>("max",
                   "(float, default 1.0) "
                   "Maximun value of uniform random")
        .SetDefault(1.0f);
    AddAttr<int>("seed",
                 "(int, default 0) "
                 "Random seed used for generating samples. "
                 "0 means use a seed generated by the system."
                 "Note that if seed is not 0, this operator will always "
                 "generate the same random numbers every time.")
        .SetDefault(0);
    AddAttr<int>("dtype", "(int, default 5(FP32)) Output tensor data type")
        .SetDefault(framework::proto::VarType::FP32);
  }
};

class UniformRandomOpVarTypeInference : public framework::VarTypeInference {
 public:
  void operator()(const framework::OpDesc& op_desc,
                  framework::BlockDesc* block) const override {
    auto out_var_name = op_desc.Output("Out").front();
    if (block->FindRecursiveOrCreateVar(out_var_name).GetType() ==
        framework::proto::VarType::SELECTED_ROWS) {
      block->FindRecursiveOrCreateVar(out_var_name)
          .SetType(framework::proto::VarType::SELECTED_ROWS);
    } else {
      block->FindRecursiveOrCreateVar(out_var_name)
          .SetType(framework::proto::VarType::LOD_TENSOR);
    }
  }
};

}  // namespace operators
}  // namespace paddle

REGISTER_OPERATOR(uniform_random, paddle::operators::UniformRandomOp,
                  paddle::operators::UniformRandomOpMaker,
                  paddle::framework::EmptyGradOpMaker,
                  paddle::operators::UniformRandomOpVarTypeInference);

REGISTER_OP_CPU_KERNEL(uniform_random,
                       paddle::operators::CPUUniformRandomKernel<float>,
                       paddle::operators::CPUUniformRandomKernel<double>);
REGISTER_OP_CPU_KERNEL(uniform_random_batch_size_like,
                       paddle::operators::CPUUniformRandomKernel<float>,
                       paddle::operators::CPUUniformRandomKernel<double>);
