#include "action.h"
#include "pddl.h"
#include "axiom.h"
#include "formula.h"
#include "object.h"
#include "predicate.h"
#include "proposition.h"
#include "state.h"
#include "utils/parameter_generator.h"
#include "derived_predicate.h"
#include "normal_form.h"

#include "np.h"
#include "addition.h"
#include "abstraction.h"

#include <iostream>
#include <fstream>
#include <string>

#include <filesystem>

namespace fs = std::filesystem;

bool createDirectories(const std::string& path) {
    try {
        fs::create_directories(path);
        return true;
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Failed to create directories!" << e.what() << std::endl;
        return false;
    }
}

class DualOutput {
public:
    DualOutput(const std::string& filename) {
        file.open(filename);
        if (!file.is_open()) {
            std::cerr << "Error opening file: " << filename << std::endl;
        }
    }

    ~DualOutput() {
        if (file.is_open()) {
            file.close();
        }
    }

    template <typename T>
    DualOutput& operator<<(const T& data) {
        buffer << data;
        file << data;
        std::cout << data;
        return *this;
    }

    DualOutput& operator<<(std::ostream& (*manipulator)(std::ostream&)) {
        buffer << manipulator;
        file << manipulator;
        std::cout << manipulator;
        return *this;
    }
private:
    std::stringstream buffer;
    std::ofstream file;
};

std::unordered_map<std::string, std::set<symbolic::Proposition>> generateFacts(symbolic::Pddl pddl) {

    std::unordered_map<std::string, std::set<symbolic::Proposition>> pred_facts;
    for (symbolic::Proposition state : pddl.initial_state()) {
        pred_facts[state.name()].insert(state);
    }
    for (auto action : pddl.actions()) {
        for (auto del_eff : action.postconditions()->del_effects) {
            if (pred_facts.count(del_eff->prop->head->getName()) != 0)
                pred_facts.erase(del_eff->prop->head->getName());
        }
        for (auto add_effect : action.postconditions()->add_effects) {
            if (pred_facts.count(add_effect->prop->head->getName()) != 0)
                pred_facts.erase(add_effect->prop->head->getName());
        }

    }
    return pred_facts;
}

int main(int argc, char* argv[])
{
    std::string domain_pddl;
    std::string problem_pddl;
    std::string addition_file;
    std::string domain_directory;
    std::string problem_name;
    std::string output_directory;

    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <domain-directory> <problem-name> <output-directory>" << std::endl;
        return 1;
    }
    domain_directory = argv[1];
    problem_name = argv[2];
    output_directory = argv[3];

    domain_pddl = domain_directory + "/domain.pddl";
    problem_pddl = domain_directory + "/" + problem_name + ".pddl";
    addition_file = domain_directory + "/addition";

    // Load and validate the PDDL.
    symbolic::Pddl pddl(domain_pddl, problem_pddl);
    pddl.IsValid(/*verbose=*/true);

    std::unordered_map<std::string, std::set<symbolic::Proposition>> pred_facts = generateFacts(pddl);
    int count_facts = 0;
    for (auto it = pred_facts.begin(); it != pred_facts.end(); ++it) {
        for (auto prop : it->second)
        {
            count_facts += 1;
        }
    }
    // read mutex_groups and baggable types from file "addition"
    std::ifstream fin(addition_file);
    auto type_preds = addition::read(fin, pddl);

    std::string res_path = output_directory + "/";
    createDirectories(res_path.c_str());
    DualOutput dualOutput(res_path + problem_name + ".info");
    std::ofstream fout_ab(res_path + problem_name + ".abs");
    std::ofstream fout_qnp = std::ofstream(res_path + problem_name + ".qnp");

    int num_ground_actions = 0;
    for (auto& action : pddl.actions()) {
        num_ground_actions += action.parameter_generator().size();
    }

    int count_grounding_props = 0;
    for (const auto& pred : pddl.predicates()) {
        count_grounding_props += pred.parameter_generator().size();
    }

    clock_t start = clock();
    // AbstractProblem(ABS) (and write subtypes and definition of variables to "problem.abs")
    auto problem_file = abstraction::AbstractProblem(pddl, type_preds, pred_facts, fout_ab, {});
    auto used_time = (double)(clock() - start) / CLOCKS_PER_SEC;

    int count_bag_obj = 0, count_notbag_obj = 0;
    for (auto obj : pddl.objects()) {
        if (type_preds.count(obj.type().name()) != 0)
            count_bag_obj += 1;
        else
            count_notbag_obj += 1;
    }

    // write information to "problem.info"
    dualOutput << "=================================" << std::endl;
    dualOutput << "#Baggable-Types: " << type_preds.size() << std::endl;
    dualOutput << "=================================" << std::endl;
    dualOutput << "#BaggableObjects/#NotBaggableObjects: " << count_bag_obj << "/" << count_notbag_obj << std::endl;
    dualOutput << "#GroundProps/#facts: " << count_grounding_props << "/" << count_facts << std::endl;
    dualOutput << "#GroundActions: " << num_ground_actions << std::endl;
    dualOutput << "=================================" << std::endl;
    dualOutput << "ABS time(s): " << std::setiosflags(std::ios::fixed) << std::setprecision(4) << used_time << std::endl;
    dualOutput << "#Subtypes: " << problem_file->num_bags() << std::endl;
    dualOutput << "=================================" << std::endl;
    dualOutput << "#Numerical-Variables: " << problem_file->num_numeric_features() << std::endl;
    dualOutput << "#Propositional-Variables: " << problem_file->num_boolean_features() << std::endl;
    dualOutput << "#Abstract-Actions: " << problem_file->num_actions() << std::endl;

    // dump BQNP problem to "problem.qnp"
    problem_file->dump(fout_qnp);

    return 0;
}
