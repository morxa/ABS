#include "np.h"
#include <iomanip>

namespace NP {
	typedef std::unordered_map<const NP::Feature*, bool> State;
	typedef std::vector<std::pair<State, const Action*>> Policy;

	Graph ConstructGraph(std::unordered_set<Graph::Node*> nodes) {
		std::unordered_map<Graph::Node*, Graph::Node*> _map;
		std::unordered_set<Graph::Node*> new_nodes;
		new_nodes.reserve(nodes.size());
		for (auto& node : nodes) {
			Graph::Node* new_node = new Graph::Node(node);
			_map[node] = new_node;
			new_nodes.insert(new_node);
		}

		for (auto& node : nodes) {
			for (auto& successor : node->successors) {
				if (nodes.count(successor) > 0) {
					_map[node]->successors.insert(_map[successor]);
				}
			}
		}
		Graph graph;
		for (auto new_node : new_nodes)
			graph.addNode(new_node);
		return graph;
	}

	Graph ConstructGraph(AndOrGraph& graph) {

		Graph new_graph;
		new_graph.addNode(graph.init);
		for (auto node : graph.nodes) {
			if (node->marked != nullptr) {
				new_graph.addEdge(node->marked);
			}
		}
		return new_graph;
	}

	bool Sieve(Graph& graph) {
		if (graph.nodes.size() <= 1)
			return true;
		bool Terminating = true;

		Tarjan tarjan(graph);
		for (auto& scc : tarjan.SCCs) {
			bool hasEdgesToDelete = false;
			bool finished = false;
			while (!finished) {
				std::set<const Feature*> inc_features;
				std::set<const Feature*> dec_features;
				std::unordered_map<const Feature*, std::vector<Graph::Node*>> _map;
				for (auto& node : scc.nodes) {
					if (node->action == nullptr)
						continue;
					auto inc = node->action->increments();
					auto dec = node->action->decrements();
					inc_features.insert(inc.begin(), inc.end());
					dec_features.insert(dec.begin(), dec.end());
					for (auto& e : dec)
						_map[e].push_back(node);
				}
				finished = true;
				for (auto& f : dec_features) {
					if (inc_features.count(f) == 0) {
						hasEdgesToDelete = true;
						finished = false;
						for (auto& node : _map[f])
							scc.deleteEdgeFrom(node);
					}
				}
			}
 
			if (hasEdgesToDelete == false)
				Terminating = Terminating && scc.nodes.size() == 1;
			else
				Terminating = Terminating && Sieve(scc);

			if (Terminating == false)
				return false;
		}

		graph.Recycling();
		assert(Terminating == true);
		return Terminating;
	}

	bool SieveForBQNP(Graph& graph) {
		if (graph.nodes.size() <= 1)
			return true;
		bool Terminating = true;

		Tarjan tarjan(graph);
		for (auto& scc : tarjan.SCCs) {
			bool hasEdgesToDelete = false;
			bool finished = false;
			while (!finished) {

				std::unordered_map<const Feature*, int> inc_features;
				std::unordered_map<const Feature*, int> dec_features;

				std::unordered_map<const Feature*, std::vector<Graph::Node*>> _map;
				for (auto& node : scc.nodes) {
					if (node->action == nullptr)
						continue;
					auto inc = node->action->increments();
					auto dec = node->action->decrements();
					for (auto& e : inc) {
						if (inc_features.count(e) == 0)
							inc_features[e] = 1;
						else
							inc_features.at(e)++;
					}
					for (auto& e : dec) {
						if (dec_features.count(e) == 0)
							dec_features[e] = 1;
						else
							dec_features.at(e)++;
					}

					for (auto& e : dec)
						_map[e].push_back(node);
				}
				finished = true;
				for (auto& f_v : dec_features) {
					if (inc_features.count(f_v.first) == 0 ) {
						hasEdgesToDelete = true;
						finished = false;
						for (auto& node : _map[f_v.first])
							scc.deleteEdgeFrom(node);
					}
					else {
						if (IsSimpleLoop(scc)) {
							if (inc_features.at(f_v.first) < f_v.second) {
								hasEdgesToDelete = true;
								finished = false;
								for (auto& node : _map[f_v.first])
									scc.deleteEdgeFrom(node);
							}
						}
					}
				}
			}

			if (hasEdgesToDelete == false)
				Terminating = Terminating && scc.nodes.size() == 1;
			else
				Terminating = Terminating && Sieve(scc);

			if (Terminating == false)
				return false;
		}

		graph.Recycling();
		assert(Terminating == true);
		return Terminating;
	}

	bool IsSimpleLoop(Graph& graph) {
		for (auto& node : graph.nodes) {
			if (node->successors.count(node) > 0)
				graph.deleteEdge(node, node);
			if (node->successors.size() > 1)
				return false;
		}
		return true;
	}
	
	bool AndORGraphSearch(AndOrGraph& graph, AndOrGraph::Node* cur,
		std::unordered_set<AndOrGraph::Connector*>& new_marked, 
		bool(*test)(Graph&)) {

		if (cur->isGoal)
			return true;
		if (cur->connectors.size() == 0)
			return false;

		for (auto conn : cur->connectors) {
			cur->marked = conn;
			bool needSieve = false;
			for (auto succ : conn->outgoings) {
				if (succ->marked != nullptr) {
					needSieve = true;
				}
			}
			if (needSieve) {
				Graph g = ConstructGraph(graph);
				if (test(g) == false)
					continue;
			}

			new_marked.insert(conn);
			bool success = true;
			for (auto succ : conn->outgoings) {
				std::unordered_set<AndOrGraph::Connector*> tmp;
				if (succ->marked == nullptr)
					success = success && AndORGraphSearch(graph, succ, tmp, test);
				if (success == false)
					break;
				else
					new_marked.insert(tmp.begin(), tmp.end());
			}

			if (success == false) {
				for (auto conn : new_marked)
					conn->incoming->marked = nullptr;
				new_marked.clear();
			}
			else
				return true;
		}

		assert(new_marked.size() == 0);
		cur->marked = nullptr;
		return false;
	}

	bool AndORGraphSearchNoPrunning(AndOrGraph& graph, AndOrGraph::Node* cur,
		std::unordered_set<AndOrGraph::Connector*>& new_marked,
		bool(*test)(Graph&) ) {

		if (cur->isGoal)
			return true;
		if (cur->connectors.size() == 0)
			return false;

		for (auto conn : cur->connectors) {
			cur->marked = conn;

			new_marked.insert(conn);
			bool success = true;
			for (auto succ : conn->outgoings) {
				std::unordered_set<AndOrGraph::Connector*> tmp;
				if (succ->marked == nullptr)
					success = success && AndORGraphSearchNoPrunning(graph, succ, tmp, test);
				if (success == false)
					break;
				else
					new_marked.insert(tmp.begin(), tmp.end());
			}

			if (success == false) {
				for (auto conn : new_marked)
					conn->incoming->marked = nullptr;
				new_marked.clear();
			}
			else {
				if (cur == graph.init) {
					Graph g = ConstructGraph(graph);
					if (test(g) == false) {
						for (auto conn : new_marked)
							conn->incoming->marked = nullptr;
						new_marked.clear();
						continue;
					}
						
					else 
						return true;
				}else
					return true;
			}	
		}

		//assert(new_marked.size() == 0);
		cur->marked = nullptr;
		return false;
	}
	
	bool NPSolver(Problem* problem, Policy& policy, bool pruning,
		bool(*test)(Graph&),double & solve_time ) {

		// Construct AndOrGraph
		AndOrGraph state_space;
		state_space.init = state_space.addNode(problem->init(),
			problem->isGoalState(problem->init()));

		std::queue<AndOrGraph::Node*> que;
		std::unordered_set<AndOrGraph::Node*> visited;

		que.push(state_space.init);

		while (!que.empty()) {
			auto cur = que.front();	que.pop();
			if (visited.count(cur) > 0)
				continue;
			visited.insert(cur);

			if (cur->isGoal)
				continue;
			auto actions = problem->ApplicableActions(cur->state);


			for (auto& action : actions) {	
				auto states = problem->ApplyAction(cur->state, action);
				std::unordered_set<AndOrGraph::Node*> nodes;
				for (auto& state : states)
					nodes.insert(state_space.addNode(state, problem->isGoalState(state)));
				state_space.addConnector(cur, nodes, action);
				for (auto& node : nodes) {
					if (visited.count(node) == 0)
						que.push(node);
				}
			}
		}

		// Search AndOrGraph
		clock_t start = clock();
		std::unordered_set<AndOrGraph::Connector*> marked;
		bool solved;
		
		if(!pruning)
			solved = AndORGraphSearchNoPrunning(state_space, state_space.init, marked, test);
		else
			solved = AndORGraphSearch(state_space, state_space.init, marked, test);

		solve_time = (double)(clock() - start) / CLOCKS_PER_SEC;
	    std::cout << "time(s): "<< std::setiosflags(std::ios::fixed)<< std::setprecision(4)  << 
			solve_time;

		policy.clear();

		if (solved) {
			for (auto conn : marked)
				policy.emplace_back(conn->incoming->state, conn->action);
			state_space.Recycling();
			return true;
		}
		else {
			state_space.Recycling();
			return false;
		}
	}

	void dump(std::ostream& os, Policy& policy) {
		for (auto& p : policy) {
			os << "{ ";
			using pairFp_b = std::pair<const NP::Feature* const, bool>;
			auto f_vComparator = [](const pairFp_b a, const pairFp_b b) { return (a.first->name()) < (b.first->name()); };
			std::set<pairFp_b,decltype(f_vComparator)> state_set(p.first.begin(), p.first.end(), f_vComparator);
			for (auto& f_v : state_set) {
				if (f_v.first->is_numeric())
					os << f_v.first->name() << (f_v.second ? ">0 " : "=0 ");
				else
					os << f_v.first->name() << (f_v.second ? "=T " : "=F ");
			}
			os << "} : " << p.second->name() << "\n";
		}
	}

	Action* Action::read(std::istream& is,
		const std::map<std::string, const Feature*>& feature_map) {
		
		std::string name;
		//is >> name;
		getline(is, name);
		getline(is, name);
		
		Action* action = new Action(name);

		int num_preconditions;
		is >> num_preconditions;
		for (int i = 0; i < num_preconditions; ++i) {
			std::string name;
			bool value;
			is >> name >> value;
			std::map<std::string, const Feature*>::const_iterator it = feature_map.find(name);
			if (it == feature_map.end())
				std::cout << "error: inexistent feature '" << name << "'" << std::endl;
			else
				action->add_precondition(it->second, value);
		}

		int num_effects;
		is >> num_effects;
		for (int i = 0; i < num_effects; ++i) {
			std::string name;
			bool value;
			is >> name >> value;
			std::map<std::string, const Feature*>::const_iterator it = feature_map.find(name);
			if (it == feature_map.end())
				std::cout << "error: inexistent feature '" << name << "'" << std::endl;
			else
				action->add_effect(it->second, value);
		}
		return action;
	}

	void Action::dump(std::ostream& os) const {
		os << name_ << std::endl << preconditions_.size();
		for (size_t i = 0; i < preconditions_.size(); ++i) {
			os << " " << *(preconditions_[i].first) << " " << preconditions_[i].second;
		}
		os << std::endl << effects_.size();
		for (size_t i = 0; i < effects_.size(); ++i) {
			os << " " << *(effects_[i].first) << " " << effects_[i].second;
		}
		os << std::endl;
	}

	Problem* Problem::read(std::istream& is) {
		std::string name;
		std::getline(is, name);
		Problem* problem = new Problem(name);

		// features_
		int num_features;
		is >> num_features;
		for (int i = 0; i < num_features; ++i) {
			Feature* feature = Feature::read(is);
			problem->add_feature(feature);
		}

		// init_
		int num_init;
		is >> num_init;
		for (int i = 0; i < num_init; ++i) {
			std::string name;
			bool value;
			is >> name >> value;
			std::map<std::string, const Feature*>::const_iterator it = problem->feature_map().find(name);
			if (it == problem->feature_map().end())
				std::cout << "error: inexistent feature '" << name << "'" << std::endl;
			else
				problem->add_init(it->second, value);
		}

		// goal_
		int num_goal;
		is >> num_goal;
		for (int i = 0; i < num_goal; ++i) {
			std::string name;
			bool value;
			is >> name >> value;
			std::map<std::string, const Feature*>::const_iterator it = problem->feature_map().find(name);
			if (it == problem->feature_map().end())
				std::cout << "error: inexistent feature '" << name << "'" << std::endl;
			else
				problem->add_goal(it->second, value);
		}

		// actions_
		int num_actions;
		is >> num_actions;
		for (int i = 0; i < num_actions; ++i) {
			Action* action = Action::read(is, problem->feature_map());
			problem->add_action(action);
			problem->incremented_features_.insert(action->increments().begin(), action->increments().end());
			problem->decremented_features_.insert(action->decrements().begin(), action->decrements().end());
		}
		return problem;
	}

	void Problem::dump(std::ostream& os) const {
		os << name_ << std::endl;

		os << features_.size();
		for (size_t i = 0; i < features_.size(); ++i) {
			os << " " << features_[i]->name() << " " << features_[i]->is_numeric();
		}
		os << std::endl;

		os << init_.size();
		for (size_t i = 0; i < init_.size(); ++i) {
			os << " " << init_[i].first->name() << " " << init_[i].second;
		}
		os << std::endl;

		os << goal_.size();
		for (size_t i = 0; i < goal_.size(); ++i) {
			os << " " << goal_[i].first->name() << " " << goal_[i].second;
		}
		os << std::endl;

		os << actions_.size() << std::endl;
		for (size_t i = 0; i < actions_.size(); ++i) {
			os << *actions_[i];
		}
	}

	std::vector<const Action*> Problem::ApplicableActions(const State& state) {
		std::vector<const Action*> res;
		for (auto& action : actions_) {
			bool valid = true;
			auto preconditions = action->preconditions();
			for (auto& pre : preconditions) {
				if (state.at(pre.first) != pre.second) {
					valid = false;
					break;
				}
			}
			if (valid) res.push_back(action);
		}
		return res;
	}

	void Product(std::vector<std::vector<std::pair<const Feature*, bool>>>& groups,
		std::vector<std::vector<std::pair<const Feature*, bool>>>& res, int layer,
		std::vector<std::pair<const Feature*, bool>> tmp) {
		if (layer == groups.size()) {
			res.push_back(tmp);
			return;
		}
		for (int i = 0; i < groups[layer].size(); i++) {
			tmp.push_back(groups[layer][i]);
			Product(groups, res, layer + 1, tmp);
			tmp.pop_back();
		}
	}

	std::vector<State> Problem::ApplyAction(const State& state, const Action* action) {
		auto effects = action->effects();

		std::vector<std::vector<std::pair<const Feature*, bool>>> groups, decs;
		for (auto feature : action->decrements()) {
			groups.push_back({ {feature, true},{feature, false} });
		}
		Product(groups, decs, 0, {});
		assert(decs.size() == (1 << groups.size()));

		std::vector<State> res;
		for (int i = 0; i < decs.size(); i++) {
			State new_state = state;
			for (auto& effect : effects) {
				if (effect.first->is_boolean())
					new_state.at(effect.first) = effect.second;
			}
			for (auto& feature : action->increments())
				new_state.at(feature) = true;

			for (auto& effect : decs[i])
				new_state.at(effect.first) = effect.second;

			res.push_back(new_state);
		}

		return res;
	}

	bool Problem::isGoalState(const State& state) {
		for (auto& f_v : goal_) {

			if (state.at(f_v.first) != f_v.second)
				return false;
		}
		return true;
	}

} // namespace NP

