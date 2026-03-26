#pragma once
#include "abstraction.h"

namespace abstraction {

	NP::Problem* AbstractProblem(const symbolic::Pddl& pddl,
		const TypePredicatesGroupsMap& type_preds, 
		std::unordered_map<std::string, std::set<symbolic::Proposition>> pred_facts,
		std::ostream& os, const std::set <std::vector< std::string >> & compact_actions) {

		std::vector<std::string> baggable_types;
		for (auto it = type_preds.begin(); it != type_preds.end(); it++) 
			baggable_types.push_back(it->first); 


		std::vector<BaggableObjects> allBaggableObjects; 
		for (int i = 0; i < baggable_types.size(); i++) { 
			std::string type_name = baggable_types[i];
			allBaggableObjects.emplace_back(
				GoalEquivalent(type_name, type_preds.at(type_name), pddl)); 
		}

		std::unordered_map<std::string, std::string> bag_st_map = buildBagStMap(allBaggableObjects);

		// Replace Facts with bags
		for (auto it = pred_facts.begin(); it != pred_facts.end(); ++it) {
			std::set<symbolic::Proposition> props = it->second;
			for (symbolic::Proposition prop : props)
			{
				it->second.erase(prop);
				ReplacedBy(prop, allBaggableObjects);
				it->second.insert(prop);
			}

		}

		std::vector<Feature> allNumericalFeatures; 
		std::vector<Feature> allBooleanFeatures;


		BagPropsGroupsMap Eavs = GenerateEavs(allBaggableObjects, type_preds, 
			pred_facts, allNumericalFeatures, allBooleanFeatures, pddl);

		std::unordered_map<std::set<symbolic::Proposition>, Feature*, my_hash::hash_set<Proposition>> sets_Fptr_map;

		GenerateFeatures(allBaggableObjects, type_preds, 
			pred_facts,allNumericalFeatures, allBooleanFeatures, sets_Fptr_map,pddl);

		std::unordered_map<std::string, std::set<int>> bag_nidx_map;
		for (auto itE = Eavs.begin(); itE != Eavs.end(); itE++)
		{
			string bag_name = itE->first;
			std::set<std::set<Proposition>> bag_attrs = itE->second;
			for(std::set<Proposition> bag_attr : bag_attrs)
			{
				for (auto& NF : allNumericalFeatures) {
					auto it = std::find_if(allNumericalFeatures.begin(),
						allNumericalFeatures.end(),
						[bag_attr](Feature& f) {return f.bags_ptr->bag_attrs[f.bag_idx - 1][f.attr_idx - 1] == bag_attr; });
					if(it!=allNumericalFeatures.end())
						bag_nidx_map[bag_name].insert(it - allNumericalFeatures.begin());
				}
			}
		}

		NP::Problem* problem = new NP::Problem((pddl.symbol()->the_domain->name + "-"
			+ pddl.symbol()->the_problem->name));

		os << "===========================\nSubtypes:\n\n";
		for (auto& bags : allBaggableObjects)
		{
			outPut(os,bags,bag_st_map) << std::endl;
			//os << bags << std::endl;
			for (int i = 0; i < bags.bags.size(); i++) {
				auto& bag = bags.bags[i];
				problem->add_bag(bags.bags[0][0].name());
			}
		}
		std::cout << "num_bags: " << problem->num_bags() << std::endl;

		symbolic::State initial_state = pddl.initial_state();
		ReplacedBy(initial_state, allBaggableObjects);

		symbolic::State goal_state = FormulaToState(pddl, pddl.goal().symbol());
		ReplacedBy(goal_state, allBaggableObjects);



		std::vector<Action_Call> action_calls =
			InstantiationAction(allBaggableObjects, pddl,pred_facts); 

		std::unordered_map<const Feature*, const NP::Feature*> feature_map;


		os << "===========================\nFeature Mapping:\n\n";
		for (int i = 0; i < allNumericalFeatures.size(); i++) {
			NP::Feature* f = new NP::Feature("N" + std::to_string(i), true);
			feature_map.insert({ &allNumericalFeatures[i], f });
			problem->add_feature(f);
			os << f->name() << " = ";
			outPut(os, allNumericalFeatures[i], bag_st_map, pddl);
			os << "\n";
		}
		std::cout << "num_numeric_features: " << problem->num_numeric_features() << std::endl;

		for (int i = 0; i < allBooleanFeatures.size(); i++) {
			NP::Feature* f = new NP::Feature("B" + std::to_string(i), false);
			feature_map.insert({ &allBooleanFeatures[i], f });
			problem->add_feature(f);
			os << f->name() << " = ";
			outPut(os, allBooleanFeatures[i], bag_st_map, pddl);
			os << "\n";
		}

		std::cout << "num_boolean_features: " << problem->num_boolean_features() << std::endl;

		std::set<std::string> compact_action_fullnames;
		for (std::vector< std::string > action_params : compact_actions) {
			std::string action_fullname = "";
			action_fullname = action_params[0] + "("; 
			bool first = true;
			for (int i = 1; i < action_params.size(); ++i) {
				if (first) {
					first = false;
				}
				else {
					action_fullname += ", ";
				}
				symbolic::Object obj(pddl, action_params[i]);
				for (auto& baggableObjects : allBaggableObjects) { 
						if (obj.type().name() == baggableObjects.type_name)
							ReplacedBy(obj, baggableObjects); 
				}
				action_fullname += obj.name();
			}
			action_fullname = action_fullname + ")";
			compact_action_fullnames.insert(action_fullname);
		}

		std::set<std::string> all_numerical_preds_name;
		for (auto it = type_preds.begin(); it != type_preds.end(); ++it) {
			for (auto preds : it->second) {
				for (auto pred : preds) {
					all_numerical_preds_name.insert(pred.name());
				}
			}
		}

		for (int i = 0; i < action_calls.size(); i++) {
			for (auto action : AbstractAction(action_calls[i],
				allNumericalFeatures, allBooleanFeatures,
				feature_map, allBaggableObjects, bag_nidx_map, type_preds,pred_facts, all_numerical_preds_name))
			{
				if(compact_action_fullnames.empty() || compact_action_fullnames.count(action->name())!=0) 
				problem->add_action(action);
			}
		}
		std::cout << "num_actions:" << problem->num_actions() << std::endl;

		auto abstract_init = AbstractState(initial_state, false,
			allNumericalFeatures, allBooleanFeatures,
			feature_map, allBaggableObjects,type_preds,
			bag_nidx_map);
		for (auto& f_value : abstract_init)
			problem->add_init(f_value.first, f_value.second);

		auto abstract_goal = AbstractState(
			goal_state,
			true,
			allNumericalFeatures, allBooleanFeatures,
			feature_map, allBaggableObjects,type_preds,
			bag_nidx_map);
		for (auto& f_value : abstract_goal)
			problem->add_goal(f_value.first, f_value.second);

		//problem->dump(std::cout);
		return problem;



	}

	std::vector<std::vector<symbolic::Object>> generateAllParams(const symbolic::Predicate& pred) {
		std::vector<std::vector<symbolic::Object>> all_params;
		for (const auto& param_pair : pred.parameter_generator()) {
			std::vector<symbolic::Object> all_param;
			for (const auto& param : param_pair)
				all_param.emplace_back(param);
			all_params.emplace_back(all_param);
		}
		return all_params;
	}

	std::vector<NP::Action*> AbstractAction(const Action_Call& action_call,
		const std::vector<Feature>& allNumericalFeatures,
		const std::vector<Feature>& allBooleanFeatures,
		const std::unordered_map<const Feature*, const NP::Feature*>& feature_map,
		const std::vector<BaggableObjects>& allBaggableObjects,
		const std::unordered_map<std::string, std::set<int>> bag_nidx_map,
		const TypePredicatesGroupsMap& type_preds,
		const PredPropsSetMap & pred_facts,
		std::set<std::string> all_numerical_preds_name
	) {

		std::vector<std::pair<const NP::Feature*, bool>> preconditions_;
		std::vector<std::pair<const NP::Feature*, bool>> effect_;
		vector<NP::Action*> abstract_actions;

		std::set<Proposition> pos_numeric_precondition_probs(action_call.pos_precondition_probs.begin(), action_call.pos_precondition_probs.end());
		for (auto& prop : action_call.pos_precondition_probs) {
			bool exist_feature = false;
			if (all_numerical_preds_name.count(prop.name()) == 0) {
				pos_numeric_precondition_probs.erase(prop);
			}
			else
				exist_feature = true; // numerical feature

			for (int i = 0; i < allBooleanFeatures.size(); i++) {
				if (prop == allBooleanFeatures[i].prop) {
					pos_numeric_precondition_probs.erase(prop);
					const NP::Feature* f = feature_map.at((&allBooleanFeatures[i]));
					preconditions_.push_back({ f ,true });
					exist_feature = true; // boolean feature
				}
			}
			if (!exist_feature && pred_facts.count(prop.name()) == 0) // there's a prop without any corresponding feature && not a omitted fact
				return abstract_actions; // return {}
		}

		for (auto& prop : action_call.neg_precondition_probs) {
			for (int i = 0; i < allBooleanFeatures.size(); i++) {
				if (prop == allBooleanFeatures[i].prop) {
					const NP::Feature* f = feature_map.at((&allBooleanFeatures[i]));
					preconditions_.push_back({ f , false });
				}
			}
		}

		std::set<Proposition> add_numeric_effect_probs(action_call.add_effect_probs.begin(), action_call.add_effect_probs.end());
		for (auto& prop : action_call.add_effect_probs) {
			for (int i = 0; i < allBooleanFeatures.size(); i++) {
				if (prop == allBooleanFeatures[i].prop) {
					add_numeric_effect_probs.erase(prop);
					const NP::Feature* f = feature_map.at((&allBooleanFeatures[i]));
					effect_.push_back({ f , true });
				}
			}
		}
		
		std::set<Proposition> del_numeric_effect_probs(action_call.del_effect_probs.begin(), action_call.del_effect_probs.end());
		for (auto& prop : action_call.del_effect_probs) {
			for (int i = 0; i < allBooleanFeatures.size(); i++) {
				if (prop == allBooleanFeatures[i].prop) {
					del_numeric_effect_probs.erase(prop);
					const NP::Feature* f = feature_map.at((&allBooleanFeatures[i]));
					effect_.push_back({ f , false });
				}
			}
		}


		symbolic::State ac_state;
		for (auto& prop : action_call.pos_precondition_probs)
			ac_state.insert(prop);
		
		std::vector<std::vector<int>> all_args_bag;
		for (auto arg : action_call.action_args) {
			if (bag_nidx_map.count(arg.name()) == 0)continue;
			all_args_bag.emplace_back(vector(bag_nidx_map.at(arg.name()).begin(), bag_nidx_map.at(arg.name()).end()));
		}

		if (all_args_bag.empty()) {
			std::string action_name = action_call.action_call_;
			action_name.erase(std::remove(action_name.begin(), action_name.end(), ' '), action_name.end());
			auto ac_ptr = new NP::Action(action_name);
			abstract_actions.emplace_back(ac_ptr);
			// boolean
			for (auto& pre : preconditions_)
				ac_ptr->add_precondition(pre.first, pre.second);
			for (auto& eff : effect_)
				ac_ptr->add_effect(eff.first, eff.second);
			return abstract_actions;
		}

		std::vector<std::set<int>> all_pos_combination;
		Product(all_args_bag,all_pos_combination,0,{});

		int action_count = 0;

		for (std::set<int> pos_combination : all_pos_combination) { 
			if (pos_combination.empty())continue;
			bool sat_flag = true;
			auto it = pos_combination.begin();
			auto getAttrSet = [&allNumericalFeatures](int i)->std::set<Proposition> {
				Feature F = allNumericalFeatures[i];
				std::set<Proposition> union_attr = F.bags_ptr->bag_attrs[F.bag_idx - 1][F.attr_idx - 1];
				return union_attr;
				};
			std::set<Proposition> pos_props_states = getAttrSet(*it);
			it++;
			while (it != pos_combination.end()) {
				std::set<Proposition> tmp = getAttrSet(*it);
				std::set_union(tmp.begin(), tmp.end(), pos_props_states.begin(), pos_props_states.end(), std::inserter(pos_props_states, pos_props_states.begin()));
				it++;
			}

			if (!std::includes(pos_props_states.begin(), pos_props_states.end(), pos_numeric_precondition_probs.begin(), pos_numeric_precondition_probs.end())) {
				sat_flag = false;
			}

			if (singleInconsistent(pos_props_states, type_preds))
				sat_flag = false;
				
			if (sat_flag) {
				std::string new_action_name = "";
				new_action_name += action_call.name() + "(";
				auto it_vn_index = pos_combination.begin();
				new_action_name += "(N" + to_string(*it_vn_index++);
				while(it_vn_index != pos_combination.end()) {
					new_action_name += ",N" + to_string(*it_vn_index++);
				}
				new_action_name += ")";

				for (auto nb : action_call.action_args) {
					if (bag_nidx_map.count(nb.name()) == 0) {
						new_action_name += "," + nb.name();
					}
				}

				new_action_name += ")";
				auto ac_ptr = new NP::Action(new_action_name);
				abstract_actions.push_back(ac_ptr);

				for (auto& pre : preconditions_)
					ac_ptr->add_precondition(pre.first, pre.second);
				for (auto& eff : effect_)
					ac_ptr->add_effect(eff.first, eff.second);
				
				std::set<int> inc_attrs;
				std::set<Proposition> final_props;

				for (int num_idx : pos_combination) {
					const Feature* feature = &allNumericalFeatures[num_idx];
					const NP::Feature* f = feature_map.at(feature);
					ac_ptr->add_precondition(f, true); 

					std::set<Proposition> current_attr_set = getAttrSet(num_idx);
					bool marked_dec = false;

					for (auto& current_attr : getAttrSet(num_idx)) {
						if (singleInconsistent(
							add_numeric_effect_probs,
							std::set<Proposition>({ current_attr }),
							type_preds
						)) {
							current_attr_set.erase(current_attr);
							marked_dec = true;
						}
					}
					
					
					for (auto& del_numeric_effect : del_numeric_effect_probs) {
						if (current_attr_set.count(del_numeric_effect) != 0) {
							current_attr_set.erase(del_numeric_effect);
							marked_dec = true;
							//break;
						}
					}
					
					if (marked_dec) {
						ac_ptr->add_effect(f, false);
					}

					
					std::set_union(current_attr_set.begin(), current_attr_set.end(),
						final_props.begin(), final_props.end(),
						std::inserter(final_props, final_props.begin()));

					
				}
				
				std::set_union(add_numeric_effect_probs.begin(), add_numeric_effect_probs.end(),
					final_props.begin(), final_props.end(),
					std::inserter(final_props, final_props.begin()));

				
				for (auto& arg_bag : all_args_bag) {
					for (int i : arg_bag) {
						
						if (pos_combination.count(i) != 0)continue;
						std::set<Proposition> new_attr_set = getAttrSet(i);
						if (std::includes(final_props.begin(), final_props.end(), 
										new_attr_set.begin(), new_attr_set.end()))
							inc_attrs.insert(i);
					}
				}

				for (int i : inc_attrs) {
					const Feature* feature = &allNumericalFeatures[i];
					const NP::Feature* f = feature_map.at(feature);
					ac_ptr->add_effect(f, true);
				}
				

			}
		}


		return abstract_actions;
	}

	std::vector<std::vector<const Feature*>> GenerateNumericalFeaturePres(
		const symbolic::State& state,
		const std::vector<Feature>& allNumericalFeatures,
		const std::vector<Feature>& allBooleanFeatures,
		const std::unordered_map<const Feature*, const NP::Feature*>& feature_map,
		const std::vector<BaggableObjects>& allBaggableObjects) {

		std::vector<std::vector<const Feature*>> numericalFeaturesGroups;
		for (int i = 0; i < allBaggableObjects.size(); i++) {
			bool type_related = false;
			for (auto& prop : state) {
				if (BagRelated(prop, allBaggableObjects[i]) != -1) {
					type_related = true;
					break;
				}
			}
			if (type_related == false)
				continue;

			std::vector<const Feature*> group;
			for (auto& feature : allNumericalFeatures) {
				if (feature.bags_ptr == &allBaggableObjects[i]) {

					auto& attr = allBaggableObjects[i].bag_attrs[feature.bag_idx - 1][feature.attr_idx - 1];

					std::vector<symbolic::Proposition> bag_props;
					for (auto& prop : state) {
						if (BagRelated(prop, allBaggableObjects[i]) != feature.bag_idx)
							continue;
						else
							bag_props.push_back(prop);
					}
					if (bag_props.size() == 0)
						continue;

					bool imply = true;
					for (auto& prop : bag_props) {
						if (attr.count(prop) == 0) {
							imply = false;
							break;
						}
					}
					if (imply)
						group.push_back(&feature);
				}
			}
			if (group.size() > 0)
				numericalFeaturesGroups.push_back(group);
		}

		std::vector<std::vector<const Feature*>> numericalFeaturesPres;
		Product(numericalFeaturesGroups, numericalFeaturesPres, 0, {});

		return numericalFeaturesPres;
	}

	std::vector<const Feature*> GenerateNumericalFeatureState(
		const symbolic::State& state,
		const std::vector<Feature>& allNumericalFeatures,
		const std::vector<Feature>& allBooleanFeatures,
		const std::unordered_map<const Feature*, const NP::Feature*>& feature_map,
		const std::vector<BaggableObjects>& allBaggableObjects) {

		std::vector<const Feature*> res;
		for (int i = 0; i < allBaggableObjects.size(); i++) {
			bool type_related = false;
			for (auto& prop : state) {
				if (BagRelated(prop, allBaggableObjects[i]) != -1) {
					type_related = true;
					break;
				}
			}
			if (type_related == false)
				continue;

			for (auto& feature : allNumericalFeatures) {
				if (feature.bags_ptr == &allBaggableObjects[i]) {

					auto& attr = allBaggableObjects[i].bag_attrs[feature.bag_idx - 1][feature.attr_idx - 1];

					std::vector<symbolic::Proposition> bag_props;
					for (auto& prop : state) {
						if (BagRelated(prop, allBaggableObjects[i]) != feature.bag_idx)
							continue;
						else
							bag_props.push_back(prop);
					}
					if (bag_props.size() == 0)
						continue;

					if (bag_props.size() < attr.size())
						throw std::runtime_error("GenerateNumericalFeatureState(): More Propositions are needed.");
					bool imply = true;
					for (auto& a : attr) {
						bool exist = false;
						for (auto& p : bag_props) {
							if (p == a) {
								exist = true;
								break;
							}
						}
						if (!exist) {
							imply = false;
							break;
						}
					}
					if (imply)
						res.push_back(&feature);
				}
			}
		}
		return res;
	}


	std::vector<std::pair<const NP::Feature*, bool>> AbstractState(
		const symbolic::State state,
		bool is_partial_state,
		const std::vector<Feature>& allNumericalFeatures,
		const std::vector<Feature>& allBooleanFeatures,
		const std::unordered_map<const Feature*, const NP::Feature*>& feature_map,
		const std::vector<BaggableObjects>& allBaggableObjects,
		const TypePredicatesGroupsMap& type_preds,
		std::unordered_map<std::string, std::set<int>> bag_nidx_map) {

		std::set<Proposition> numeric_props;
		std::vector< std::pair<const NP::Feature*, bool>> abstract_state;
		for (auto& prop : state) {
			numeric_props.insert(prop);
			for (auto& feature : allBooleanFeatures) {
				if (prop == feature.prop) {
					const NP::Feature* f = feature_map.at((&feature));
					abstract_state.push_back({ f, true });
					numeric_props.erase(prop);
					break;
				}
			}
		}
		if (!is_partial_state) {
			for (auto& feature : allBooleanFeatures) {
				if (!state.contains(feature.prop)) {
					const NP::Feature* f = feature_map.at((&feature));
					abstract_state.push_back({ f, false });
				}
			}
		}


		if (!is_partial_state) {
			for (const Feature& NF : allNumericalFeatures) {
				std::set<Proposition> union_attr = NF.bags_ptr->bag_attrs[NF.bag_idx - 1][NF.attr_idx - 1];
				const NP::Feature* f = feature_map.at(&NF);
				if (std::includes(numeric_props.begin(), numeric_props.end(), union_attr.begin(), union_attr.end())) {
					abstract_state.push_back({ f, true });
				}
				else {
					abstract_state.push_back({ f, false });
				}
			}
		}
		else {
			std::set<int> negs_in_goal;
			for (auto it = bag_nidx_map.begin(); it != bag_nidx_map.end(); ++it) {
				string bag_name = it->first;
				std::set<Proposition> bag_related_props;
				for (auto& prop : numeric_props) {
					for (auto& obj : prop.arguments()) {
						if (obj.name() == bag_name) {
							bag_related_props.insert(prop);
							break;
						}
					}
				}
				for (int i : it->second) {
					const Feature & NF = allNumericalFeatures[i];
					std::set<Proposition> union_attr = NF.bags_ptr->bag_attrs[NF.bag_idx - 1][NF.attr_idx - 1];
					if (!std::includes(union_attr.begin(), union_attr.end(), bag_related_props.begin(), bag_related_props.end() )) {
						negs_in_goal.insert(i);
					}
				}
				
			}
			for (int i : negs_in_goal) {
				const Feature& NF = allNumericalFeatures[i];
				const NP::Feature* f = feature_map.at(&NF);
				abstract_state.push_back({ f, false });
			}

		}

		return abstract_state;
	}


	std::vector<symbolic::Object> AbstractObject(
		const symbolic::Pddl& pddl,
		const std::vector<BaggableObjects>& allBaggableObjects) {

		std::vector<symbolic::Object> objects;
		for (auto& baggableObjects : allBaggableObjects) {
			for (auto& bag : baggableObjects.bags)
				objects.push_back(bag[0]);
		}
		for (auto& obj : pddl.objects()) {
			bool baggable = false;
			for (auto& baggableObjects : allBaggableObjects) {
				if (obj.type().name() != baggableObjects.type_name)
					continue;
				else {
					baggable = true;
					break;
				}
			}
			if (!baggable)
				objects.push_back(obj);
		}
		return objects;
	}

	std::vector<Action_Call> InstantiationAction(
		const std::vector<BaggableObjects> allBaggableObjects,
		const symbolic::Pddl& pddl,
		const PredPropsSetMap& pred_facts
	) {
		std::unordered_map<std::string, vector<symbolic::Object>> baggable_objs_map;
		for (auto BaggableObjects : allBaggableObjects) {
			for (auto bag : BaggableObjects.bags) {
				baggable_objs_map[BaggableObjects.type_name].emplace_back(bag[0]);
			}
		}

		vector<Action_Call> action_calls;

		const std::vector<Action>& Actions = pddl.actions();
		for (auto& action : Actions) {
			std::vector<std::vector<symbolic::Proposition>> props_groups;
			std::vector<std::vector<symbolic::Object>> argobjs_groups;
			for(auto prop : Action_Call(action, action.parameters(), pddl).pos_precondition_probs){
				if (pred_facts.count(prop.name()) != 0) {
					props_groups.emplace_back(pred_facts.at(prop.name()).begin(), pred_facts.at(prop.name()).end());
					argobjs_groups.emplace_back(prop.arguments());
				}
			}
			std::vector<std::unordered_map<std::string, symbolic::Object>> res;
			getPossArgs(props_groups, argobjs_groups, res, 0, {});
			for(auto arg_map : res)
			{
				vector<vector<symbolic::Object>> ais_vector;
				for (auto ai : action.parameters()) {
					if (arg_map.count(ai.name()) == 0) {
						if(baggable_objs_map.count(ai.type().name())!=0)
						{
							ais_vector.emplace_back(baggable_objs_map.at(ai.type().name()));
						}
						else {
							ais_vector.emplace_back(pddl.object_map().at(ai.type().name()));
						}
					}
					else {
						ais_vector.emplace_back(vector<symbolic::Object>{ arg_map.at(ai.name()) });
					}
				}
				vector<vector<symbolic::Object>> all_params;
				Product(ais_vector,all_params,0,{});
				for (auto params : std::set(all_params.begin(), all_params.end())) {
					Action_Call action_call(action, params, pddl);

					std::set<Proposition> inter_set,
						set1(action_call.add_effect_probs.begin(), action_call.add_effect_probs.end()),
						set2(action_call.del_effect_probs.begin(), action_call.del_effect_probs.end());
					std::set_intersection(set1.begin(), set1.end(),
						set2.begin(), set2.end(),
						std::inserter(inter_set, inter_set.begin()));
					if (!inter_set.empty())continue;

					action_calls.emplace_back(action_call);
				}
			}
		}
		return action_calls;
	}




	void getPossArgs(const std::vector<std::vector<symbolic::Proposition>>& props_groups,
		const std::vector<std::vector<symbolic::Object>>& argobjs_groups,
		std::vector<std::unordered_map<std::string, symbolic::Object>>& res,
		int layer, 
		std::unordered_map<std::string, symbolic::Object> tmp
	) {
		assert(props_groups.size() == argobjs_groups.size());
		if (layer == props_groups.size()) {
			res.push_back(tmp);
			return;
		}

			for (const auto & prop : props_groups[layer]) {
				bool safe = true;
				vector<std::string> new_argnames;
				for (int i = 0; i < prop.arguments().size(); ++i) {
					std::string argname = argobjs_groups[layer][i].name();
					if (tmp.count(argname) == 0) {
						new_argnames.emplace_back(argname);
						tmp[argname] = prop.arguments()[i];
					}
					else if(tmp.at(argname) != prop.arguments()[i]) {
						safe = false;
						break;
					}
					
				}
				if (safe) {
					getPossArgs(props_groups, argobjs_groups, res, layer + 1, tmp);
				}
				{
					for (std::string new_argname : new_argnames) {
						tmp.erase(new_argname);
					}
				}
			}

	}

	symbolic::State FormulaToState(const symbolic::Pddl& pddl,
		const VAL::goal* symbol) {

		const auto* simple_goal = dynamic_cast<const VAL::simple_goal*>(symbol);
		if (simple_goal != nullptr) {
			symbolic::Formula f(pddl, simple_goal);
			return symbolic::State(pddl, { f.to_string() });
		}

		const auto* conj_goal = dynamic_cast<const VAL::conj_goal*>(symbol);
		if (conj_goal != nullptr) {
			const VAL::goal_list* goals = conj_goal->getGoals();
			symbolic::State state(pddl, {});
			for (const VAL::goal* goal : *goals) {
				symbolic::Formula f(pddl, goal);
				state.insert(symbolic::Proposition(pddl, f.to_string()));
			}
			return state;
		}
		throw std::runtime_error("FormulaToState():  not implemented.");
	}

	BaggableObjects GoalEquivalent(
		const string type_name,
		const vector<vector<Predicate>>& pred_groups,  
		const Pddl& pddl) {

		State goal_state = FormulaToState(pddl, pddl.goal().symbol());

		std::vector<symbolic::Object> objects = pddl.object_map().at(type_name); 
		std::vector<Attribute> objects_attr;
		for (auto& obj : objects) {
			std::vector<symbolic::Proposition> attr;
			for (auto& prop : goal_state) { 
				if (ObjectRelated(prop, obj)) 
					attr.emplace_back(prop);
			}
			objects_attr.emplace_back(obj, attr);
		}

		return  Partition(objects, objects_attr); 
	}

	std::unordered_map<std::string, std::string> buildBagStMap(const std::vector<BaggableObjects>& allBaggableObjects) {
		std::unordered_map<std::string, std::string> bag_st_map;
		int index = 1;
		for (BaggableObjects type_bag : allBaggableObjects) {
			std::string type_name = type_bag.type_name;
			for (auto bag : type_bag.bags) {
				bag_st_map[bag[0].name()] = "st" + std::to_string(index) + "(" + type_name + ")";
				index += 1;
			}
		}
		return bag_st_map;
	}

	std::ostream& outPut(std::ostream& os,
		const BaggableObjects& baggableObjects,
		std::unordered_map<std::string, std::string> bag_st_map) {

		os << "Subtypes of " << baggableObjects.type_name << ":\n";
		for (int i = 0; i < baggableObjects.bags.size(); i++) {
			auto& bag = baggableObjects.bags[i];
			os << "\t" << bag_st_map.at(bag[0].name()) << " = " << "\{ ";
			for (auto& obj : bag)
				os << obj.name() << " ";
			os << "\}\n";
		}
		return os;

	}

	std::ostream& outPut(std::ostream& os,
		const Feature& f,
		std::unordered_map<std::string, std::string> bag_st_map,const Pddl & pddl) {
		if (f.numerical)
		{
			std::stringstream eavs;
			std::set<std::string > sts;
			for (auto& prob : f.bags_ptr->bag_attrs[f.bag_idx - 1][f.attr_idx - 1]) {
				eavs << prob.name();
				eavs << "(";
				std::string separator;
				for (auto& obj : prob.arguments()) {
					if (bag_st_map.find(obj.name()) != bag_st_map.end()) {
						sts.insert(bag_st_map.at(obj.name()));
						eavs << separator << obj.type().name();
					}
					else {
						eavs << separator << obj.name();
					}
					if (separator.empty()) separator = ", ";
				}
				eavs << ")" << " ";
			}

			os << "( " << "\{ ";
			for (std::string st : sts) {
				os << st << " ";
			}
			os << "} , { " << eavs.str();
			os << "} )\n";
		}
		else
			os << f.prop << "\n";
		return os;

	}

	bool ObjectRelated(const symbolic::Proposition& prop,
		const symbolic::Object& obj) {

		for (auto& arg : prop.arguments()) {
			if (arg.symbol() == obj.symbol())
				return true;
		}
		return false;
	}

	int BagRelated(const symbolic::Proposition& prop,
		const BaggableObjects& baggableObjects) {
		auto& bags = baggableObjects.bags;
		for (int i = 0; i < bags.size(); i++) {
			if (ObjectRelated(prop, bags[i][0]))
				return i + 1;
		}
		return -1;
	}

	bool TypeRelated(const symbolic::Predicate& pred,
		const std::string& type_name) {
		std::vector<symbolic::Object> params = pred.parameters();
		for (symbolic::Object& param : params) {
			if (type_name == param.type().name())
				return true;
		}
		return false;
	}

	bool PropostionSimilar(const symbolic::Proposition& lhs,
		const symbolic::Proposition& rhs, const std::string& type_name) {

		assert(lhs.name() == rhs.name()); 
		int len = lhs.arguments().size();
		const std::vector<Object>& lhs_args = lhs.arguments();
		const std::vector<Object>& rhs_args = rhs.arguments();
		for (int i = 0; i < len; i++) {
			if (lhs_args[i].type().name() == type_name) 
				continue;
			if (lhs_args[i].symbol() != rhs_args[i].symbol())
				return false;
		}
		return true;
	}

	bool AttributeSimilar(
		const Attribute& lhs, 
		const Attribute& rhs, 
		const std::string& type_name) {

		for (auto& l : lhs.attr) {
			bool exist_r = false;
			for (auto& r : rhs.attr) {
				if (l.name() == r.name()) {
					exist_r = true;
					if (PropostionSimilar(l, r, type_name) == false)
						return false;
				}
			}
			if (!exist_r)
				return false;
		}
		return true;
	}

	BaggableObjects Partition(std::vector<symbolic::Object> objects, 
		std::vector<Attribute> objects_attr) {

		if (objects.empty()) return { "null" };
		const std::string type_name = objects[0].type().name();
		BaggableObjects baggableObjects(type_name);

		while (!objects.empty()) {
			std::vector<symbolic::Object> bag;
			bag.push_back(objects[0]); 

			auto it = objects.begin() + 1;
			auto it_attr = objects_attr.begin() + 1;
			for (; it != objects.end(); ) {
				if (AttributeSimilar(objects_attr[0], *it_attr, type_name)
					&&
					AttributeSimilar(*it_attr, objects_attr[0], type_name)) {
					bag.push_back(*it);
					it = objects.erase(it); 
					it_attr = objects_attr.erase(it_attr);
				}
				else {
					it++;
					it_attr++;
				}
			}
			objects.erase(objects.begin()); 
			objects_attr.erase(objects_attr.begin());
			baggableObjects.bags.push_back(bag);
		}
		return baggableObjects;
	}

	std::ostream& operator<<(std::ostream& os, const Feature& f) {
		if (f.numerical)
		{
			//int j = 0;
				os << "\t" << "\{ ";
				for (auto& prob : f.bags_ptr->bag_attrs[f.bag_idx - 1][f.attr_idx - 1])
					os << prob << "  ";
				os << "}\n";
		}
		else
			os << f.prop << "\n";
		return os;
	}

	std::ostream& operator<<(std::ostream& os, 
		const BaggableObjects& baggableObjects) {

		os << baggableObjects.type_name << ":\n";
		for (int i = 0; i < baggableObjects.bags.size(); i++) {
			auto& bag = baggableObjects.bags[i];
			os << "\tbag" << (i + 1) << "\{ ";
			for (auto& obj : bag)
				os << obj.name() << " ";
			os << "\}\n";

			std::vector<std::set<symbolic::Proposition>> attrs = baggableObjects.bag_attrs[i];
			os << "\n\t\t\{\n";
			int j = 0;
			for (auto& attr : attrs) {
				os << "\t\t\tattr" << (++j) << "\{ ";
				for (auto& prob : attr)
					os << prob << "  ";
				os << "}\n";
			}
			os << "\t\t\}\n";
		}
		return os;
	}

	std::ostream& operator<<(std::ostream& os,
		const Action_Call& action_call) {
		os << "\n" << action_call.action_call_ << ":\n";
		os << "precondition:\n";
		for (auto& prop : action_call.pos_precondition_probs)
			cout << "\t" << prop << std::endl;
		for (auto& prop : action_call.neg_precondition_probs)
			cout << "\tnot " << prop << std::endl;

		os << "add_effect:\n";
		for (auto& add : action_call.add_effect_probs)
			os << "\t" << add << std::endl;

		os << "del_effect:\n";
		for (auto& del : action_call.del_effect_probs)
			os << "\t" << del << std::endl;

		return os;
	}

	bool propTypeRelated(symbolic::Proposition prop, std::string type_name) {
		for (const auto& arg : prop.arguments()) {
			if (arg.type().name()== type_name)
			{
				return true;
			}
		}
		return false;
	}

	bool propTypeBagRelated(symbolic::Proposition prop, std::string type_name, const std::vector<std::vector<symbolic::Predicate>> & predss) {
		for (const auto& arg : prop.arguments()) {
			if (arg.type().name() == type_name)
			{
				for (auto& preds : predss) {
					for (auto& pred : preds) {
						if (pred.name() == prop.name()) 
							return true;
					}
				}
			}
		}
		return false;
	}


	bool singleInconsistent(std::set<Proposition> props,
		const TypePredicatesGroupsMap& type_preds ) { 
		std::vector pv(props.begin(), props.end());
		for (int i = 0; i < pv.size(); ++i) {
			for (int j = i + 1; j < pv.size(); ++j) {
				for (const auto& arg : pv[i].arguments()) {
					std::string i_type_name = arg.type().name();
					if (type_preds.count(i_type_name) == 0)continue;
					if (!propTypeBagRelated(pv[j], i_type_name, type_preds.at(i_type_name)))continue;
					else {
						if (pv[i].name() == pv[j].name()) return true; 
					}
					for (auto mutex_group : type_preds.at(i_type_name)) {
						if (std::find_if(mutex_group.begin(), mutex_group.end(), [=](Predicate pred) {return pv[i].name() == pred.name(); }) != mutex_group.end()) {
							{
								if (std::find_if(mutex_group.begin(), mutex_group.end(), [=](Predicate pred) {return pv[j].name() == pred.name(); }) != mutex_group.end()) {
									return true;
								}
								else {
									break;
								}
							}
						}
					}
				}
			}
		}
		return false;
	}

	bool singleInconsistent(std::set<Proposition> props1,
		std::set<Proposition> props2,
		const TypePredicatesGroupsMap& type_preds) { 
		std::set<Proposition> temp;
		std::set_union(props1.begin(), props1.end(), props2.begin(), props2.end(), std::inserter(temp, temp.begin()));
		return singleInconsistent(temp,type_preds);
	}

	BagPropsGroupsMap GenerateEavs(
		std::vector<BaggableObjects>& allBaggableObjects, 
		const TypePredicatesGroupsMap& type_preds,
		const PredPropsSetMap& pred_facts,
		std::vector<Feature>& allNumericalFeatures,
		std::vector<Feature>& allBooleanFeatures,
		const symbolic::Pddl& pddl){

		set<symbolic::Proposition> ground_prop_with_bag_set; 
		// Numerical Features
		for (auto it = type_preds.begin(); it != type_preds.end(); it++) { 
			const std::string type_name = it->first; 
			const std::vector<std::vector<symbolic::Predicate>>
				pred_mutex_groups = it->second; 

			auto bags_iter = std::find_if(allBaggableObjects.begin(), allBaggableObjects.end(),
				[&type_name](const BaggableObjects& item) {
					return item.type_name == type_name; }); 
			bags_iter->bag_attrs.reserve(bags_iter->bags.size());

			
			for (int i = 0; i < bags_iter->bags.size(); i++) { 
				std::vector<std::vector<symbolic::Proposition>> prob_mutex_groups;
				for (auto& pred_mutex_group : pred_mutex_groups) { 
					std::set<symbolic::Proposition> prob_mutex_group;
					for (auto& pred : pred_mutex_group) {  
						if (pred_facts.count(pred.name()) != 0) {
							for (auto prob: pred_facts.at(pred.name())) {
								int bag_idx = BagRelated(prob, *bags_iter);
								if (bag_idx != i + 1) continue; 
								prob_mutex_group.insert(prob);
								ground_prop_with_bag_set.insert(prob);
							} 
						}
						else
						{
							auto& param_gen = pred.parameter_generator(); 
							for (auto& params : param_gen) { 
								symbolic::Proposition prob(pred.name(), params);
								ReplacedBy(prob, allBaggableObjects); 
								int bag_idx = BagRelated(prob, *bags_iter);
								if (bag_idx != i + 1) continue; 
									prob_mutex_group.insert(prob);
									ground_prop_with_bag_set.insert(prob);
							}
						}
					}
					prob_mutex_groups.push_back(std::vector<symbolic::Proposition> 
						(prob_mutex_group.begin(), prob_mutex_group.end()));
				}
				bags_iter->bag_attrs.push_back({});
				Product(prob_mutex_groups, bags_iter->bag_attrs[i], 0, {}); 
			}
		}

		std::unordered_map<std::string, std::set<std::set<Proposition>>> Avs_all;

		for (auto BaggableObjects : allBaggableObjects) {
			std::string type_name = BaggableObjects.type_name;
			for (int i = 0; i < BaggableObjects.bags.size(); ++i) {
				Avs_all[BaggableObjects.bags[i][0].name()] = std::set(BaggableObjects.bag_attrs[i].begin(), BaggableObjects.bag_attrs[i].end());
			}
		}


		vector<symbolic::Proposition> ground_prop_with_bag(ground_prop_with_bag_set.begin(), ground_prop_with_bag_set.end());
		while(true){ 
			auto tmp_a = Avs_all;
			for (auto& prop : ground_prop_with_bag) { 
				std::set<std::set<Proposition>> Avs_p;
				std::vector<std::vector<std::set<Proposition>>> all_Avs_tp;
				std::set<std::string> T_p;
				for (auto& arg : prop.arguments()) {
					std::string bag_name = arg.name();
					if (Avs_all.find(bag_name) == Avs_all.end()) continue;
					std::set<std::set<Proposition>> Avs_copy = Avs_all[bag_name];
					std::set<std::set<Proposition>> Avs_tp;
					for (auto avs_t : Avs_all[bag_name]) { 
						if (std::find_if(avs_t.begin(), avs_t.end(), [prop](Proposition prop_)->bool {return prop == prop_; }) != avs_t.end()) { 
							Avs_tp.insert(avs_t);
							Avs_copy.erase(avs_t);
						}
					}
					Avs_all[bag_name] = Avs_copy;
					if (!Avs_tp.empty())
					{
						T_p.insert(bag_name);
						all_Avs_tp.emplace_back(std::vector(Avs_tp.begin(), Avs_tp.end()));
					}
				}

				if (T_p.empty())continue;

				std::vector<std::vector<std::set<Proposition>>> all_Avs_tp_product; 
				Product(all_Avs_tp, all_Avs_tp_product, 0, {});
				for (auto& product_sets : all_Avs_tp_product) {
					//assert(product_sets.size() > 0);
					auto it = product_sets.begin();
					std::set<Proposition> tmp = *it;
					it++;
					while (it != product_sets.end()) {
						std::set_union(it->begin(), it->end(), tmp.begin(), tmp.end(), std::inserter(tmp, tmp.begin()));
						it++;
					}
					if (!singleInconsistent(tmp,type_preds)) 
						Avs_p.insert(tmp);
				}

				for (std::string bag_name : T_p) {
					std::set<std::set<Proposition>> tmp;
					std::set_union(Avs_all[bag_name].begin(), Avs_all[bag_name].end(), Avs_p.begin(), Avs_p.end(), std::inserter(tmp, tmp.begin()));
					Avs_all[bag_name] = tmp;
				}
			}
			if (Avs_all == tmp_a)break;
			else { tmp_a = Avs_all; }
		}

		while (true) { 
			auto fixed_Avs_all = Avs_all;
			for (auto itAvs = Avs_all.begin(); itAvs != Avs_all.end(); ++itAvs) {
				std::string current_bag_name = itAvs->first;
				std::set<std::set<Proposition>> bag_propss_copy = itAvs->second;
				for (auto props : bag_propss_copy) {
					itAvs->second.erase(props);
					std::set<std::string> related_bags;
					for (auto prop : props) {
						for (auto obj : prop.arguments()) {
							if (obj.name() == current_bag_name)continue;
							if (fixed_Avs_all.count(obj.name()) != 0) {
								related_bags.insert(obj.name());
							}
						}
					}
					std::vector<std::vector<std::set<Proposition>>> related_bags_propsss;
					for (const std::string& related_bag : related_bags) {
						related_bags_propsss.emplace_back(std::vector<std::set<Proposition>>(fixed_Avs_all.at(related_bag).begin(),
							fixed_Avs_all.at(related_bag).end()));
					}
					related_bags_propsss.emplace_back(std::vector<std::set<Proposition>>({ props }));
					std::vector<std::vector<std::set<Proposition>>> all_related_props_product; 
					Product(related_bags_propsss, all_related_props_product, 0, {});

					for (auto& product_sets : all_related_props_product) {
						auto it = product_sets.begin();
						std::set<Proposition> tmp = *it;
						it++;
						while (it != product_sets.end()) {
							std::set_union(it->begin(), it->end(), tmp.begin(), tmp.end(), std::inserter(tmp, tmp.begin()));
							it++;
						}
						if (!singleInconsistent(tmp, type_preds)) 
							itAvs->second.insert(tmp);
					}



				}
			}

			if (Avs_all == fixed_Avs_all)break;
			else { fixed_Avs_all = Avs_all; }
		}


		for (auto & BaggableObjects : allBaggableObjects) {
			for (int i = 0; i < BaggableObjects.bags.size(); ++i) {
				std::set<std::set<symbolic::Proposition>>& tmp = Avs_all[BaggableObjects.bags[i][0].name()];
				BaggableObjects.bag_attrs[i] = std::vector(tmp.begin(),tmp.end());
			}
		}

		return Avs_all;

	}

	bool consistentWithFacts(const symbolic::Proposition & prop,const std::unordered_map<std::string, std::set<symbolic::Proposition>>& pred_facts) {
		if (pred_facts.count(prop.name()) == 0)return true;
		if (pred_facts.at(prop.name()).count(prop) != 0)return true;
		else {
			return false;
		}
	}

	void GenerateFeatures(
		std::vector<BaggableObjects>& allBaggableObjects, 
		const TypePredicatesGroupsMap& type_preds,
		const PredPropsSetMap& pred_facts,
		std::vector<Feature>& allNumericalFeatures,
		std::vector<Feature>& allBooleanFeatures,
		std::unordered_map<std::set<symbolic::Proposition>, Feature*, my_hash::hash_set<Proposition>> &sets_Fptr_map, 
		const symbolic::Pddl& pddl) {

		
		for (auto& baggableObjects : allBaggableObjects) {
			for (int i = 0; i < baggableObjects.bags.size(); i++) {
				for (int j = 0; j < baggableObjects.bag_attrs[i].size(); j++) {
					if (baggableObjects.bag_attrs[i][j].empty())continue;
					if (sets_Fptr_map.count(baggableObjects.bag_attrs[i][j]) == 0) {
						allNumericalFeatures.push_back(
							Feature(true, baggableObjects, i + 1, j + 1));
						sets_Fptr_map[baggableObjects.bag_attrs[i][j]] = &allNumericalFeatures.back(); 
						
					}
				}
			}
			
		}

		// Boolean Features
		std::vector<std::string> baggable_types;
		for (auto it = type_preds.begin(); it != type_preds.end(); it++)
			baggable_types.push_back(it->first);
		std::vector<symbolic::Proposition> booleanFeatures;
		const std::vector<symbolic::Predicate>& preds = pddl.predicates();
		for (const symbolic::Predicate& pred : preds) {
			if (pred_facts.count(pred.name()) != 0)
				continue;


			if (pred.parameters().size() == 0) {
				booleanFeatures.push_back(symbolic::Proposition(pred.name(), {}));
				continue;
			}

			bool pure_bool = true;
			auto& param_gen = pred.parameter_generator();
			for (std::vector<symbolic::Object> param : param_gen) {
				symbolic::Proposition gen_prop(pred.name(), param);
				for (auto it = type_preds.begin(); it != type_preds.end(); ++it) {
					if (propTypeBagRelated(gen_prop, it->first,type_preds.at(it->first))) {
						pure_bool = false;
						break;
					}
				}
				if (pure_bool)
					booleanFeatures.push_back(gen_prop);
			}

		}
		for (auto& prop : booleanFeatures)
			allBooleanFeatures.push_back(Feature(false, prop));
	}


	void ReplacedBy(symbolic::Object& obj,
		const BaggableObjects& baggableObjects) {

		std::vector<std::vector<symbolic::Object>> bags = 
			baggableObjects.bags;
		for (int i = 0; i < bags.size(); i++) { 
			for (int j = 0; j < bags[i].size(); j++) { 
				if (bags[i][j] == obj) 
					obj = bags[i][0]; 
			}
		}
	}

	void ReplacedBy(symbolic::Proposition& prob,
		const std::vector<BaggableObjects>& allBaggableObjects) {

		std::vector<std::string> types;
		for (auto it = allBaggableObjects.begin(); it != allBaggableObjects.end(); it++)
			types.push_back(it->type_name);

		std::vector<symbolic::Object> params = prob.arguments();
		for (auto& baggableObjects : allBaggableObjects) { 
			for (auto& obj : params) { 
				if (obj.type().name() == baggableObjects.type_name)
					ReplacedBy(obj, baggableObjects); 
			}
		}
		prob = symbolic::Proposition(prob.name(), params);
	}

	void ReplacedBy(symbolic::State& state,
		const std::vector<BaggableObjects>& allBaggableObjects) {

		symbolic::State state_;
		for (symbolic::Proposition prop : state) {
			ReplacedBy(prop, allBaggableObjects);
			if (state_.contains(prop)) 
				continue;
			state_.insert(prop);
		}
		state = state_;
	}

	void ReplacedBy(symbolic::Action& action,
		const std::vector<BaggableObjects>& allBaggableObjects) {

		std::string action_call = "";
		std::vector<symbolic::Object> params = action.parameters();
		for (auto& baggableObjects : allBaggableObjects) { 
			for (auto& obj : params) { 
				if (obj.type().name() == baggableObjects.type_name)
					ReplacedBy(obj, baggableObjects); 
			}
		}
		
	}


	template<class T>
	void Product(const std::vector<std::vector<T>>& groups,
		std::vector<std::vector<T>>& res,
		int layer, std::vector<T> tmp) {
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

	template<class T>
	void Product(const std::vector<std::vector<T>>& groups,
		std::vector<std::set<T>>& res,
		int layer, std::vector<T> tmp) {
		if (layer == groups.size()) {
			res.push_back(std::set(tmp.begin(), tmp.end()));
			return;
		}

		for (int i = 0; i < groups[layer].size(); i++) {
			tmp.push_back(groups[layer][i]);
			Product(groups, res, layer + 1, tmp);
			tmp.pop_back();
		}
	}

	void Product(const std::vector<std::vector<symbolic::Proposition>>& groups,
		std::vector<std::set<symbolic::Proposition>>& res,
		int layer, std::vector<symbolic::Proposition> tmp) {
		if (layer == groups.size()) {
			res.push_back(std::set(tmp.begin(), tmp.end())); 
			return;
		}

		for (int i = 0; i < groups[layer].size(); i++) {
			tmp.push_back(groups[layer][i]);
			Product(groups, res, layer + 1, tmp); 
			tmp.pop_back();
		}
	}

	void Product(const std::vector<std::vector<const Feature*>>& groups,
		std::vector<std::vector<const Feature*>>& res,
		int layer, std::vector <const Feature*> tmp) {
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


	void Apply_Objects(
		const symbolic::Pddl& pddl,
		Action_Call& action_call) {

		//precondition_probs
		VAL::goal* symbol = action_call.symbol()->precondition;

		const auto* simple_goal = dynamic_cast<const VAL::simple_goal*>(symbol);
		if (simple_goal != nullptr) {
			const VAL::proposition* prop = simple_goal->getProp();
			const std::string& name_predicate = prop->head->getNameRef();
			const std::vector<symbolic::Object> prop_params = Object::CreateList(pddl, prop->args);
			auto apply = symbolic::Formula::CreateApplicationFunction(
				action_call.parameters(), prop_params);
			symbolic::Proposition new_prob(name_predicate, apply(action_call.action_args));
			action_call.pos_precondition_probs.push_back(new_prob);
		}

		const auto* conj_goal = dynamic_cast<const VAL::conj_goal*>(symbol);
		if (conj_goal != nullptr) {
			const VAL::goal_list* goals = conj_goal->getGoals();
			for (const VAL::goal* goal : *goals) {

				const auto* pos = dynamic_cast<const VAL::simple_goal*>(goal);
				if (pos != nullptr) {
					const VAL::proposition* prop = pos->getProp();
					const std::string& name_predicate = prop->head->getNameRef();
					const std::vector<symbolic::Object> prop_params = Object::CreateList(pddl, prop->args);
					auto apply = symbolic::Formula::CreateApplicationFunction(
						action_call.parameters(), prop_params);
					symbolic::Proposition new_prob(name_predicate, apply(action_call.action_args));
					action_call.pos_precondition_probs.push_back(new_prob);
					continue;
				}

				const auto* neg = dynamic_cast<const VAL::neg_goal*>(goal);
				if (neg != nullptr) {
					const auto* simple = dynamic_cast<const VAL::simple_goal*>(neg->getGoal());
					if (simple != nullptr) {
						const VAL::proposition* prop = simple->getProp();
						const std::string& name_predicate = prop->head->getNameRef();
						const std::vector<symbolic::Object> prop_params = Object::CreateList(pddl, prop->args);
						auto apply = symbolic::Formula::CreateApplicationFunction(
							action_call.parameters(), prop_params);
						symbolic::Proposition new_prob(name_predicate, apply(action_call.action_args));
						action_call.neg_precondition_probs.push_back(new_prob);
						continue;
					}
					throw std::runtime_error("FormulaToState():  not implemented.");
				}
				throw std::runtime_error("FormulaToState():  not implemented.");
			}
		}
		//add_effect_probs
		auto& add_effects = action_call.symbol()->effects->add_effects;
		for (auto& e : add_effects) {
			const VAL::proposition* prop = e->prop;
			const std::string& name_predicate = prop->head->getNameRef();
			const std::vector<symbolic::Object> prop_params = Object::CreateList(pddl, prop->args);
			auto apply = symbolic::Formula::CreateApplicationFunction(
				action_call.parameters(), prop_params);
			symbolic::Proposition new_prob(name_predicate, apply(action_call.action_args));
			action_call.add_effect_probs.push_back(new_prob);
		}

		//delete_effect_probs
		auto& del_effects = action_call.symbol()->effects->del_effects;
		for (auto& e : del_effects) {
			const VAL::proposition* prop = e->prop;
			const std::string& name_predicate = prop->head->getNameRef();
			const std::vector<symbolic::Object> prop_params = Object::CreateList(pddl, prop->args);
			auto apply = symbolic::Formula::CreateApplicationFunction(
				action_call.parameters(), prop_params);
			symbolic::Proposition new_prob(name_predicate, apply(action_call.action_args));
			action_call.del_effect_probs.push_back(new_prob);
		}
	}
}// namespace abstraction

