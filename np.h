#ifndef NP_H
#define NP_H

#include <string>
#include <iostream>
#include <map>
#include <set>
#include <queue>
#include <vector>
#include <cassert>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <ctime>



namespace NP {
	class AndOrGraph;
	class Graph;
	class Tarjan;
	class Feature;
	class Action;
	class Problem;
	typedef std::unordered_map<const NP::Feature*, bool> State;
	typedef std::vector<std::pair<State, const Action*>> Policy;


	class AndOrGraph {
	public:
		class Node;
		class Connector;
		Node* init;
		std::unordered_set<Node*> nodes;
		std::unordered_set<const Connector*> connectors;
		//std::unordered_map<Node*, bool> visited;

		Node* addNode(const State& state, bool isGoal) {
			for (auto& node : nodes) {
				if (state == node->state) {
					return node;
				}
			}
			Node* node = new Node(state, isGoal);
			nodes.insert(node);
			return node;
		}

		void addConnector(Node* incoming, std::unordered_set<Node*> outgoings,
			const Action* action) {
			Connector* conn = new Connector(incoming, outgoings, action);
			connectors.insert(conn);
			incoming->connectors.insert(conn);
		}

		void Recycling() {
			for (auto node : nodes)
				delete node;
			for (auto conn : connectors)
				delete conn;
		}

		class Node {
		public:
			Node(const State& state, bool isGoal)
				:state(state), isGoal(isGoal), marked(nullptr) {}

			bool isGoal;
			State state;
			std::unordered_set<Connector*> connectors;
			Connector* marked;
		};

		class Connector {
		public:
			Connector(Node* incoming, std::unordered_set<Node*> outgoings,
				const NP::Action* action)
				:incoming(incoming), outgoings(outgoings), action(action) {}

			Node* incoming;
			std::unordered_set<Node*> outgoings;
			const Action* action;
		};
	};

	class Graph {
	public:
		class Node;
		std::unordered_set<Node*> nodes;

		Node* addNode(const AndOrGraph::Node* _node) {
			for (auto& node : nodes) {
				if (node->state == _node->state)
					return node;
			}
			Node* node = new Node(_node);
			nodes.insert(node);
			return node;
		}

		void addEdge(const AndOrGraph::Connector* _conn) {
			auto from = addNode(_conn->incoming);
			for (auto& _node : _conn->outgoings)
				from->successors.insert(addNode(_node));
		}

		Node* addNode(Node* node) {
			if (nodes.count(node) == 0)
				nodes.insert(node);
			return node;
		}

		void deleteEdge(Node* from, Node* to) {
			from->successors.erase(to);
			if (from->successors.size() == 0)
				from->action = nullptr;
		}

		void deleteEdgeFrom(Node* from) {
			from->successors.clear();
			from->action = nullptr;
		}

		void Recycling() {
			for (auto node : nodes)
				delete node;
		}

		class Node {
		public:
			Node(const Node* node) :state(node->state), action(node->action) {}
			Node(const AndOrGraph::Node* _node)
				:state(_node->state), action(nullptr) {
				if (_node->marked != nullptr) {
					action = _node->marked->action;
				}
			}

			std::unordered_set<Node*> successors;
			const Action* action;
			State state;
		};
	};

	Graph ConstructGraph(std::unordered_set<Graph::Node*> nodes);
	Graph ConstructGraph(AndOrGraph& graph);
	
	class Tarjan {
	public:
		std::vector<Graph> SCCs;
		int timeStamp;
		std::unordered_map<Graph::Node*, int> DFN;
		std::unordered_map<Graph::Node*, int> LOW;
		std::stack<Graph::Node*> Stack;
		std::unordered_set<Graph::Node*> InStack;

		Tarjan(Graph& graph) {
			timeStamp = 0;
			for (auto node : graph.nodes) {
				DFN[node] = timeStamp;
				LOW[node] = timeStamp;
			}

			for (auto node : graph.nodes) {
				if (DFN[node] == 0)
					RunTarjan(node);
			}
		}

		void RunTarjan(Graph::Node* node) {
			timeStamp++;
			DFN[node] = timeStamp;
			LOW[node] = timeStamp;
			Stack.push(node);
			InStack.insert(node);

			for (auto succ : node->successors) {
				if (DFN[succ] == 0) {
					RunTarjan(succ);
					LOW[node] = std::min(LOW[node], LOW[succ]);
				}
				else {
					if (InStack.count(succ) > 0)
						LOW[node] = std::min(LOW[node], DFN[succ]);
				}
			}

			if (DFN[node] == LOW[node]) {
				std::unordered_set<Graph::Node*> NodeSet;
				Graph::Node* tmp = nullptr;
				while (tmp != node) {
					tmp = Stack.top();
					Stack.pop();
					InStack.erase(tmp);
					NodeSet.insert(tmp);
				}
				SCCs.push_back(ConstructGraph(NodeSet));
			}
		}
	};
	

	bool IsSimpleLoop(Graph& graph);
	bool Sieve(Graph& graph);
	bool SieveForBQNP(Graph& graph);
	bool AndORGraphSearch(AndOrGraph& graph, AndOrGraph::Node* cur,
		std::unordered_set<AndOrGraph::Connector*>& new_marked, bool(*test)(Graph&));
	bool AndORGraphSearchNoPrunning(AndOrGraph& graph, AndOrGraph::Node* cur,
		std::unordered_set<AndOrGraph::Connector*>& new_marked, bool(*test)(Graph&));
	bool NPSolver(Problem* problem, Policy& policy, bool pruning, bool(*test)(Graph&),double & solve_time);
	void dump(std::ostream& os, Policy& policy);

	class Feature {
	protected:
		std::string name_;
		bool numeric_;
	public:
		Feature(const std::string name, bool numeric)
			:name_(name), numeric_(numeric) {}

		const std::string& name() const { return name_; }
		bool is_numeric() const { return numeric_; }
		bool is_boolean() const { return !is_numeric(); }

		static Feature* read(std::istream& is) {
			std::string name;
			bool numeric;
			is >> name >> numeric;
			return new Feature(name, numeric);
		}
		void dump(std::ostream& os) const {
			os << name_;
		}
	}; // class Feature

	class Action {
	protected:
		const std::string name_;
		std::vector<std::pair<const Feature*, bool> > preconditions_;
		std::vector<std::pair<const Feature*, bool> > effects_;
		std::set<const Feature*> increments_;
		std::set<const Feature*> decrements_;

	public:
		Action(const std::string& name) : name_(name) { }

		const std::string& name() const { return name_; }

		bool check(std::ostream& os) const {
			for (std::set<const Feature*>::const_iterator it = decrements_.begin(); it != decrements_.end(); ++it) {
				if (!is_precondition(*it, true)) {
					os << "error: bad precondition for decremented feature " << (*it)->name() << " in action " << name() << std::endl;
					return false;
				}
			}
			return true;
		}

		// preconditions
		size_t num_preconditions() const { return preconditions_.size(); }
		std::pair<const Feature*, bool> precondition(int i) const { return preconditions_[i]; }
		std::vector<std::pair<const Feature*, bool>>  preconditions() const { return preconditions_; }

		void add_precondition(const Feature* feature, bool value) {
			preconditions_.emplace_back(feature, value);
		}

		bool is_precondition(const Feature* f, bool value) const {
			for (size_t i = 0; i < preconditions_.size(); ++i) {
				if (preconditions_[i] == std::make_pair(f, value))
					return true;
			}
			return false;
		}

		// effects
		size_t num_effects() const { return effects_.size(); }
		std::pair<const Feature*, bool> effect(int i) const { return effects_[i]; }
		std::vector<std::pair<const Feature*, bool> > effects() const { return effects_; }

		void add_effect(const Feature* feature, bool value) {
			effects_.emplace_back(feature, value);
			if (feature->is_numeric() && value)
				increments_.insert(feature);
			else if (feature->is_numeric() && !value)
				decrements_.insert(feature);
		}

		// incremented / decremented variables
		const std::set<const Feature*>& increments() const { return increments_; }
		const std::set<const Feature*>& decrements() const { return decrements_; }

		static Action* read(std::istream& is,
			const std::map<std::string, const Feature*>& feature_map);

		virtual void dump(std::ostream& os) const;

	}; // class Action

	class Problem {
	protected:
		std::string name_;

		std::vector<const Feature*> features_;
		std::vector<const Feature*> numeric_features_;
		std::vector<const Feature*> boolean_features_;
		std::map<std::string, const Feature*> feature_map_;

		std::vector<std::pair<const Feature*, bool> > init_;
		std::vector<std::pair<const Feature*, bool> > goal_;

		std::vector<const Action*> actions_;
		std::set<const Feature*> incremented_features_;
		std::set<const Feature*> decremented_features_;



	public:

		Problem(const std::string& name) : name_(name) { }
		~Problem() {
			for (size_t i = 0; i < actions_.size(); ++i)
				delete actions_[i];
			for (size_t i = 0; i < features_.size(); ++i)
				delete features_[i];
		}

		// features_
		int num_features() const { return features_.size(); }

		int num_numeric_features() const { return numeric_features_.size(); }

		int num_boolean_features() const { return boolean_features_.size(); }

		const Feature& feature(int i) const { return *features_.at(i); }
		const std::vector<const Feature*>& features() const { return features_; }

		const Feature& numeric_feature(int i) const { return *numeric_features_.at(i); }

		const Feature& boolean_feature(int i) const { return *boolean_features_.at(i); }

		const std::map<std::string, const Feature*>& feature_map() const { return feature_map_; }

		const Feature* feature(const std::string& name) const {
			std::map<std::string, const Feature*>::const_iterator it = feature_map_.find(name);
			return it == feature_map_.end() ? nullptr : it->second;
		}

		void add_feature(const Feature* feature) {
			assert(feature != nullptr);
			assert(feature_map_.find(feature->name()) == feature_map_.end());

			features_.push_back(feature);
			if (feature->is_numeric())
				numeric_features_.push_back(feature);
			else
				boolean_features_.push_back(feature);
			feature_map_.insert(std::make_pair(feature->name(), features_.back()));
		}
		bool is_numeric_feature(const Feature* f) const {
			for (size_t i = 0; i < numeric_features_.size(); ++i) {
				if (numeric_features_[i] == f)
					return true;
			}
			return false;
		}
		bool is_boolean_feature(const Feature* f) const {
			for (size_t i = 0; i < boolean_features_.size(); ++i) {
				if (boolean_features_[i] == f)
					return true;
			}
			return false;
		}
		bool is_feature(const Feature* f) const {
			return is_numeric_feature(f) || is_boolean_feature(f);
		}


		// init_ 
		int num_inits() const { return init_.size(); }

		std::pair<const Feature*, bool> init(int i) const { return init_.at(i); }

		State init() {
			State state;
			for (auto& f_v : init_)
				state[f_v.first] = f_v.second;
			return state;
		}

		void add_init(const Feature* feature, bool value) { init_.emplace_back(feature, value); }

		// goal_
		int num_goals() const { return goal_.size(); }

		std::pair<const Feature*, bool> goal(int i) const { return goal_.at(i); }


		void add_goal(const Feature* feature, bool value) { goal_.emplace_back(feature, value); }

		// actions_
		int num_actions() const { return actions_.size(); }

		const Action& action(int i) const { return *actions_.at(i); }

		void add_action(const Action* action) {
			assert(action != nullptr);
			actions_.push_back(action);
		}
		static Problem* read(std::istream& is);

		void dump(std::ostream& os) const;


		std::vector<const Action*> ApplicableActions(const State& state);
		std::vector<State> ApplyAction(const State& state, const Action* action);
		bool isGoalState(const State& state);

	};// class Problem

} // namespace NP

inline std::ostream& operator<<(std::ostream& os, const NP::Feature& feature) {
	feature.dump(os);
	return os;
}

inline std::ostream& operator<<(std::ostream& os,
	const NP::Action& action) {
	action.dump(os);
	return os;
}

inline std::ostream& operator<<(std::ostream& os,
	const NP::Problem& problem) {
	problem.dump(os);
	return os;
}

#endif