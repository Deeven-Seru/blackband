#include <iostream>
#include <vector>
#include <stack>
#include <unordered_map>

class TransactionSnapshot {
public:
    TransactionSnapshot() : budget(0) {}
    
    void setBudget(int b) {
        budget = b;
    }
    
    int getBudget() const {
        return budget;
    }
    
    void preserveTrace(const std::string &trace) {
        traces.push_back(trace);
    }
    
    void rollbackBudget(int amount) {
        if (amount <= budget) {
            budget -= amount;
        } else {
            std::cerr << "Insufficient budget for rollback" << std::endl;
        }
    }
    
    void nestedCheckpoint() {
        checkpoints.push(budget);
    }
    
    void restore() {
        if (!checkpoints.empty()) {
            budget = checkpoints.top();
            checkpoints.pop();
        } else {
            std::cerr << "No checkpoint to restore" << std::endl;
        }
    }
    
    void printTraces() const {
        for (const auto &trace : traces) {
            std::cout << trace << std::endl;
        }
    }

private:
    int budget;
    std::vector<std::string> traces;
    std::stack<int> checkpoints;
};

int main() {
    TransactionSnapshot snapshot;
    snapshot.setBudget(100);
    
    snapshot.preserveTrace("Initialized with budget 100");
    
    snapshot.rollbackBudget(20);
    snapshot.nestedCheckpoint();
    snapshot.preserveTrace("Budget after rollback 80");
    
    snapshot.restore();
    
    std::cout << "Current Budget: " << snapshot.getBudget() << std::endl;
    snapshot.printTraces();

    return 0;
}