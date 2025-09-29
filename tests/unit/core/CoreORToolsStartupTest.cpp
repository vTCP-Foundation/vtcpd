#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ortools/linear_solver/linear_solver.h"
#include "ortools/base/version.h"
using operations_research::MPSolver;

using namespace testing;

class CoreORToolsStartupTest : public Test {
protected:
    void SetUp() override {
        // Setup for startup tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(CoreORToolsStartupTest, ORToolsStartupVerificationTest) {
    // Skip under AddressSanitizer due to prebuilt OR-Tools incompatibility with ASan
#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
    GTEST_SKIP() << "Skipping under ASan due to prebuilt OR-Tools incompatibility with ASan";
#  endif
#endif
#if defined(__SANITIZE_ADDRESS__)
    GTEST_SKIP() << "Skipping under ASan due to prebuilt OR-Tools incompatibility with ASan";
#endif
    // Test the startup verification logic that would be in Core::initORTools()
    
    // Step 1: Test solver creation
    std::unique_ptr<MPSolver> testSolver(MPSolver::CreateSolver("GLOP"));
    ASSERT_TRUE(testSolver != nullptr) << "OR-Tools GLOP solver should be available for startup";
    
    // Step 2: Test version requirements
    int major = operations_research::OrToolsMajorVersion();
    int minor = operations_research::OrToolsMinorVersion();
    
    std::cout << "OR-Tools version detected: " << major << "." << minor << std::endl;
    
    // Version should be >= 9.0 as per PRD specification
    EXPECT_GE(major, 9) << "OR-Tools version " << major << "." << minor 
                       << " is insufficient. Minimum version 9.0 required.";
    
    // Step 3: Test that we can use basic solver operations
    auto* var = testSolver->MakeNumVar(0.0, 10.0, "test_var");
    ASSERT_TRUE(var != nullptr) << "Should be able to create variables";
    
    auto* constraint = testSolver->MakeRowConstraint(0.0, 5.0, "test_constraint");
    ASSERT_TRUE(constraint != nullptr) << "Should be able to create constraints";
    
    constraint->SetCoefficient(var, 1.0);
    
    auto* objective = testSolver->MutableObjective();
    ASSERT_TRUE(objective != nullptr) << "Should be able to access objective";
    
    objective->SetCoefficient(var, 1.0);
    objective->SetMaximization();
    
    // Test solving
    MPSolver::ResultStatus status = testSolver->Solve();
    EXPECT_EQ(status, MPSolver::OPTIMAL) << "Simple test problem should solve optimally";
    
    if (status == MPSolver::OPTIMAL) {
        EXPECT_GT(objective->Value(), 0.0) << "Optimal value should be positive";
        EXPECT_LE(var->solution_value(), 5.0) << "Solution should respect constraint";
    }
    
    std::cout << "OR-Tools startup verification successful" << std::endl;
}

TEST_F(CoreORToolsStartupTest, MultipleSolverTypesTest) {
    // Skip under AddressSanitizer due to prebuilt OR-Tools incompatibility with ASan
#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
    GTEST_SKIP() << "Skipping under ASan due to prebuilt OR-Tools incompatibility with ASan";
#  endif
#endif
#if defined(__SANITIZE_ADDRESS__)
    GTEST_SKIP() << "Skipping under ASan due to prebuilt OR-Tools incompatibility with ASan";
#endif
    // Test that various solver types are available
    std::vector<std::string> solverTypes = {"GLOP", "PDLP"};
    
    for (const auto& solverType : solverTypes) {
        std::unique_ptr<MPSolver> solver(MPSolver::CreateSolver(solverType));
        if (solver) {
            std::cout << "Solver " << solverType << " is available" << std::endl;
            
            // Test basic operations with this solver
            auto* var = solver->MakeNumVar(0.0, 1.0, "test");
            EXPECT_TRUE(var != nullptr) << "Should create variables with " << solverType;
        } else {
            std::cout << "Solver " << solverType << " is not available (this may be expected)" << std::endl;
        }
    }
    
    // At least GLOP should be available for the application to work
    std::unique_ptr<MPSolver> glopSolver(MPSolver::CreateSolver("GLOP"));
    ASSERT_TRUE(glopSolver != nullptr) << "GLOP solver must be available for application functionality";
}

TEST_F(CoreORToolsStartupTest, VersionCompatibilityTest) {
    // Safe to run; no solver operations here
    // Test specific version compatibility requirements
    int major = operations_research::OrToolsMajorVersion();
    int minor = operations_research::OrToolsMinorVersion();
    
    // Test that we're not using a very old version
    EXPECT_FALSE(major < 8) << "OR-Tools version is too old (< 8.0)";
    
    // Test that version is within expected range (not too new that might break compatibility)
    EXPECT_LE(major, 15) << "OR-Tools version seems unusually high, please verify compatibility";
    
    // Log version for debugging
    std::cout << "Testing with OR-Tools version: " << major << "." << minor << std::endl;
    
    // Test that we can get version information without errors
    EXPECT_GE(major, 0) << "Version major should be non-negative";
    EXPECT_GE(minor, 0) << "Version minor should be non-negative";
}
