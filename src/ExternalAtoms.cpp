/* dlvhex -- Answer-Set Programming with external interfaces.
 * Copyright (C) 2005, 2006, 2007 Roman Schindlauer
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Thomas Krennwallner
 * Copyright (C) 2009, 2010, 2011 Peter Schüller
 * Copyright (C) 2011, 2012, 2013, 2014 Christoph Redl
 * 
 * This file is part of dlvhex.
 *
 * dlvhex is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * dlvhex is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with dlvhex; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

/**
 * @file ExternalAtoms.cpp
 * @author Christoph Redl <redl@kr.tuwien.ac.at
 *
 * @brief Implements external atoms for nested HEX-programs.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

#include "NestedHexPlugin.h"
#include "ExternalAtoms.h"
#include "dlvhex2/PlatformDefinitions.h"
#include "dlvhex2/ProgramCtx.h"
#include "dlvhex2/Registry.h"
#include "dlvhex2/Printer.h"
#include "dlvhex2/Printhelpers.h"
#include "dlvhex2/Logger.h"
#include "dlvhex2/ExternalLearningHelper.h"

#include <iostream>
#include <string>
#include <algorithm>

#include "boost/program_options.hpp"
#include "boost/range.hpp"
#include "boost/foreach.hpp"
#include "boost/filesystem.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>

DLVHEX_NAMESPACE_BEGIN

namespace nestedhex{

// ============================== Class NestedHexPluginAtom ==============================

InterpretationPtr NestedHexPluginAtom::translateInputInterpretation(InterpretationConstPtr input){

	if (!input) return InterpretationPtr(new Interpretation(reg));
	DBGLOG(DBG, "Translating interpretation: " << *input);

	RegistryPtr reg = getRegistry();

	DBGLOG(DBG, "Translating input to nested hex program");

	// Translate input from higher-order notation to ordinary input
	InterpretationPtr edb(new Interpretation(reg));
	bm::bvector<>::enumerator en = input->getStorage().first();
	bm::bvector<>::enumerator en_end = input->getStorage().end();
	while (en < en_end){
		// do not translate auxiliary input!
		if (!reg->ogatoms.getIDByAddress(*en).isExternalInputAuxiliary()){
			OrdinaryAtom oatom = reg->ogatoms.getByAddress(*en);
			// check if input is valid
			if (oatom.tuple.size() < 2) throw PluginError("Input to nested HEX programs must be of arity >= 2");
			if (!oatom.tuple[2].isTerm() || !oatom.tuple[2].isIntegerTerm()) throw PluginError("Input to nested HEX programs must contain the arity of the mapped predicate at its second position");
			if (oatom.tuple.size() < oatom.tuple[2].address + 3) throw PluginError("Input to nested HEX programs has an arity smaller than the specified one + 2");
			for (int ir = 2 + oatom.tuple[2].address + 1; ir < oatom.tuple.size(); ++ir){
				if (oatom.tuple[ir] != ctx.getPluginData<NestedHexPlugin>().theNestedHexPlugin->emptyID) throw PluginError("Input to nested HEX programs must have constant empty on all attribute positions greater than the arity of the mapped predicate");
			}

			// delete all empty elements, the 2-th and the 0-nd element
			int arity = oatom.tuple[2].address;
			oatom.tuple.erase(oatom.tuple.begin() + 2 + oatom.tuple[2].address + 1, oatom.tuple.end());
			oatom.tuple.erase(oatom.tuple.begin() + 2, oatom.tuple.begin() + 3);
			oatom.tuple.erase(oatom.tuple.begin());
			oatom.kind = ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG;
			ID inputAtom = reg->storeOrdinaryAtom(oatom);
			edb->setFact(inputAtom.address);
#ifndef NDEBUG
			std::string outstr = "Translated " + RawPrinter::toString(reg, reg->ogatoms.getIDByAddress(*en)) + " to " + RawPrinter::toString(reg, inputAtom);
			DBGLOG(DBG, outstr);
#endif
			assert(reg->ogatoms.getByID(inputAtom).tuple.size() == arity + 1 && "Translation of input atom failed");
		}
		en++;
	}
	return edb;
}

NestedHexPluginAtom::NestedHexPluginAtom(std::string predName, ProgramCtx& ctx, bool positivesubprogram) : PluginAtom(predName, positivesubprogram), ctx(ctx), positivesubprogram(positivesubprogram){
}

void NestedHexPluginAtom::NestedHexPluginAtom::retrieve(const Query& query, Answer& answer){
	assert(false && "this method should never be called since the learning-based method is present");
}

void NestedHexPluginAtom::retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods)
{
	DBGLOG(DBG, "NestedHexPlugin::retrieve");

	RegistryPtr reg = getRegistry();

	// input parameters to external atom &hex["type", "prog", p, q](x):
	//	query.input[0] (i.e. "type"): either "file" or "string"
	//	query.input[1] (i.e. "prog"): filename of the program P over which we do query answering
	//	query.input[2] (i.e. p): a predicate name; the set F of all atoms over this predicate are added to P as facts before evaluation
	//	query.input[3] (i.e. q): name of the query predicate; the external atom will be true for all output vectors x such that q(x) is true in every answer set of P \cup F

	const std::vector<InterpretationPtr>& answersets = ctx.getPluginData<NestedHexPlugin>().theNestedHexPlugin->getHexAnswer(ctx, query.input[0], query.input[1], translateInputInterpretation(query.interpretation)).answersets;

	// create a mask for the query predicate, i.e., retrieve all atoms over the query predicate
	PredicateMaskPtr pm = PredicateMaskPtr(new PredicateMask());
	pm->setRegistry(reg);
	pm->addPredicate(query.input[3]);
	pm->updateMask();

	// now since we know all answer sets, we can answer the query
	answerQuery(pm, answersets, query, answer);
}

void NestedHexPluginAtom::learnSupportSets(const Query& query, NogoodContainerPtr nogoods){

	RegistryPtr reg = getRegistry();

	// input parameters to external atom &hex["type", "prog", p, q](x):
	//	query.input[0] (i.e. "type"): either "file" or "string"
	//	query.input[1] (i.e. "prog"): filename of the program P over which we do query answering
	//	query.input[2] (i.e. p): a predicate name; the set F of all atoms over this predicate are added to P as facts before evaluation
	//	query.input[3] (i.e. q): name of the query predicate; the external atom will be true for all output vectors x such that q(x) is true in every answer set of P \cup F

	const NestedHexPlugin::HexAnswer& answer = ctx.getPluginData<NestedHexPlugin>().theNestedHexPlugin->getHexAnswer(ctx, query.input[0], query.input[1], translateInputInterpretation(query.interpretation));
	const std::vector<InterpretationPtr>& answersets = answer.answersets;

	// learn support sets (only if --supportsets option is specified on the command line)
	if (!!nogoods && !!nogoods && query.ctx->config.getOption("SupportSets")){
		SimpleNogoodContainerPtr preparedNogoods = SimpleNogoodContainerPtr(new SimpleNogoodContainer());

		// for all rules r of P
		BOOST_FOREACH (ID ruleID, answer.pc.idb){
			const Rule& rule = reg->rules.getByID(ruleID);

			// Check if r is a rule of form
			//			hatom :- B,
			// where hatom is a single atom and B contains only positive atoms.
			bool posBody = true;
			BOOST_FOREACH (ID b, rule.body){
				if (b.isNaf()) posBody = false;
			}
			if (rule.head.size() == 1 /*&& posBody*/){
				// We learn the following (nonground) nogoods: { T b | b \in B } \cup { F hatom }.
				Nogood nogood;

				// add all (positive) body atoms
				BOOST_FOREACH (ID blit, rule.body) nogood.insert(NogoodContainer::createLiteral(blit));

				// add the negated head atom
				nogood.insert(NogoodContainer::createLiteral(rule.head[0] | ID(ID::NAF_MASK, 0)));

				// actually learn this nogood
				DBGLOG(DBG, "Learn prepared nogood " << nogood.getStringRepresentation(reg));
				preparedNogoods->addNogood(nogood);
			}
		}

		// exhaustively generate all resolvents of the prepared nogoods
		DBGLOG(DBG, "Computing resolvents of prepared nogoods up to size " << (query.interpretation->getStorage().count() + 1));
		preparedNogoods->addAllResolvents(reg, query.interpretation->getStorage().count() + 1);

		// make a list of input predicates and their arities
		InterpretationPtr inputPredicates(new Interpretation(reg));
		std::map<IDAddress, int> aritiy;
		int maxarity = 0;
		bm::bvector<>::enumerator en = query.interpretation->getStorage().first();
		bm::bvector<>::enumerator en_end = query.interpretation->getStorage().end();
		while (en < en_end){
			const OrdinaryAtom& ogatom = reg->ogatoms.getByAddress(*en);
			assert(ogatom.tuple.size() >= 3 && "invalid input atom");
			inputPredicates->setFact(ogatom.tuple[1].address);
			aritiy[ogatom.tuple[1].address] = ogatom.tuple.size() - 3;
			if (aritiy[ogatom.tuple[1].address] > maxarity) maxarity = aritiy[ogatom.tuple[1].address];
			en++;
		}		

		// all nogoods of form
		//		{ T b | b \in B } \cup { F q(X) }
		// containing only atoms over p and q are transformed into support sets of form
		//		{ T b | b \in B } \cup { F e_{&testCautiousQuery["prog", p, q]}(X) }
		// This is because if all body atoms are in the input (atoms over predicate p), then q(X) is true in every answer set of P \cup F.
		// But then, since q is the query predicate, also &testCautiousQuery["prog", p, q](X) is true.
		DBGLOG(DBG, "Extracting support sets from prepared nogoods");
		for (int i = 0; i < preparedNogoods->getNogoodCount(); i++){
			const Nogood& ng = preparedNogoods->getNogood(i);
			bool isSupportSet = true;
			Nogood supportSet;
			BOOST_FOREACH (ID id, ng){
				ID pred = reg->lookupOrdinaryAtom(id).tuple[0];
				if (inputPredicates->getFact(pred.address)){
					// translate to higher-order notation
					const OrdinaryAtom& oatom = reg->ogatoms.getByAddress(id.address);
					OrdinaryAtom hoatom(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
					hoatom.tuple.push_back(query.input[2]);			// auxiliary input
					hoatom.tuple.push_back(ID::termFromInteger(maxarity));	// auxiliary input
					hoatom.tuple.insert(hoatom.tuple.end(), oatom.tuple.begin(), oatom.tuple.end());
					while  (hoatom.tuple.size() < maxarity + 2) hoatom.tuple.push_back(ctx.getPluginData<NestedHexPlugin>().theNestedHexPlugin->emptyID);
					ID hoatomID = reg->storeOrdinaryAtom(hoatom);
#ifndef NDEBUG
					std::string dbgstr = "Translated " + RawPrinter::toString(reg, reg->ogatoms.getIDByAddress(id.address)) + " to " + RawPrinter::toString(reg, hoatomID);
					DBGLOG(DBG, dbgstr);
#endif
					supportSet.insert(id);
				}else if (pred == query.input[3]){
					const OrdinaryAtom& hatom = reg->lookupOrdinaryAtom(id);
					// add e_{&testCautiousQuery["prog", p, q]}(X) using a helper function	
					supportSet.insert(NogoodContainer::createLiteral(
															ExternalLearningHelper::getOutputAtom(
																	query,	// this parameter is always the same
																	Tuple(hatom.tuple.begin() + 1, hatom.tuple.end()),	// hatom.tuple[0]=q and hatom.tuple[i] for i >= 1 stores the elements of X;
																																											// here we need only the X and use hatom.tuple.begin() + 1 to eliminate the predicate q
																	!id.isNaf() /* technical detail, is set to true almost always */).address,
															true,																								// sign of the literal e_{&testCautiousQuery["prog", p, q]}(X) in the nogood
															id.isOrdinaryGroundAtom()															/* specify if this literal is ground or nonground (the same as the head atom) */ ));
				}else{
					isSupportSet = false;
					break;
				}
			}
			if (isSupportSet){
				DBGLOG(DBG, "Learn support set: " << supportSet.getStringRepresentation(reg));
				nogoods->addNogood(supportSet);
			}
		}
	}
}

// ============================== Class CHEXAtom ==============================

CHEXAtom::CHEXAtom(ProgramCtx& ctx) : NestedHexPluginAtom("hexCautious", ctx)
{
	DBGLOG(DBG,"Constructor of hexCautious plugin is started");
	addInputConstant(); // type of the subprogram (file or string)
	addInputConstant(); // name of the subprogram
	addInputPredicate(); // specifies the input to the subprogram
	addInputConstant(); // query predicate
	setOutputArity(0); // variable

	prop.variableOutputArity = true; // the output arity of this external atom depends on the arity of the query predicate
//	prop.supportSets = true; // we provide support sets
//	prop.completePositiveSupportSets = true; // we even provide (positive) complete support sets
}

void CHEXAtom::answerQuery(PredicateMaskPtr pm, const std::vector<InterpretationPtr>& answersets, const Query& query, Answer& answer){

	RegistryPtr reg = getRegistry();

	DBGLOG(DBG, "Answer cautious query");

	// special case: if there are no answer sets, cautious ground queries are trivially true, but cautious non-ground queries are always false for all ground substituions (by definition)
	if (answersets.size() == 0){
		if (query.pattern.size() == 0){
			// return the empty tuple
			Tuple t;
			answer.get().push_back(t);
		}
	}else{

		InterpretationPtr out = InterpretationPtr(new Interpretation(reg));
		out->add(*pm->mask());

		// get the set of atoms over the query predicate which are true in all answer sets
		BOOST_FOREACH (InterpretationPtr intr, answersets){
			DBGLOG(DBG, "Inspecting " << *intr);
			out->getStorage() &= intr->getStorage();
		}

		// retrieve all output atoms oatom=q(c)
		bm::bvector<>::enumerator en = out->getStorage().first();
		bm::bvector<>::enumerator en_end = out->getStorage().end();
		while (en < en_end){
			const OrdinaryAtom& oatom = reg->ogatoms.getByAddress(*en);

			// add c to the output
			answer.get().push_back(Tuple(oatom.tuple.begin() + 1, oatom.tuple.end()));
			en++;
		}
	}
}

// ============================== Class BHEXAtom ==============================

BHEXAtom::BHEXAtom(ProgramCtx& ctx) : NestedHexPluginAtom("hexBrave", ctx)
{
	DBGLOG(DBG,"Constructor of hexBrave plugin is started");
	addInputConstant(); // type of the subprogram (file or string)
	addInputConstant(); // name of the subprogram
	addInputPredicate(); // specifies the input to the subprogram
	addInputConstant(); // query predicate
	setOutputArity(0); // variable

	prop.variableOutputArity = true; // the output arity of this external atom depends on the arity of the query predicate
//	prop.supportSets = true; // we provide support sets
//	prop.completePositiveSupportSets = true; // we even provide (positive) complete support sets
}

void BHEXAtom::answerQuery(PredicateMaskPtr pm, const std::vector<InterpretationPtr>& answersets, const Query& query, Answer& answer){

	RegistryPtr reg = getRegistry();

	DBGLOG(DBG, "Answer brave query");

	InterpretationPtr out = InterpretationPtr(new Interpretation(reg));

	// get the set of atoms over the query predicate which are true in all answer sets
	BOOST_FOREACH (InterpretationPtr intr, answersets){
		DBGLOG(DBG, "Inspecting " << *intr);
		out->getStorage() |= (pm->mask()->getStorage() & intr->getStorage());
	}

	// retrieve all output atoms oatom=q(c)
	bm::bvector<>::enumerator en = out->getStorage().first();
	bm::bvector<>::enumerator en_end = out->getStorage().end();
	while (en < en_end){
		const OrdinaryAtom& oatom = reg->ogatoms.getByAddress(*en);

		// add c to the output
		answer.get().push_back(Tuple(oatom.tuple.begin() + 1, oatom.tuple.end()));
		en++;
	}
}

// ============================== Class IHEXAtom ==============================

IHEXAtom::IHEXAtom(ProgramCtx& ctx) : NestedHexPluginAtom("hexInspection", ctx)
{
	DBGLOG(DBG,"Constructor of hexInspection plugin is started");
	addInputConstant(); // type of the subprogram (file or string)
	addInputConstant(); // name of the subprogram
	addInputPredicate(); // specifies the input to the subprogram
	addInputConstant(); // query
	addInputTuple(); // queries might require a further parameter
	setOutputArity(2); // variable

//	prop.supportSets = true; // we provide support sets
//	prop.completePositiveSupportSets = true; // we even provide (positive) complete support sets
}


void IHEXAtom::retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods)
{
	DBGLOG(DBG, "NestedHexPlugin::retrieve");

	RegistryPtr reg = getRegistry();

	// input parameters to external atom &hex["type", "prog", p, qt, qp](x):
	//	query.input[0] (i.e. "type"): either "file" or "string"
	//	query.input[1] (i.e. "prog"): filename of the program P over which we do query answering
	//	query.input[2] (i.e. p): a predicate name; the set F of all atoms over this predicate are added to P as facts before evaluation
	//	query.input[3] (i.e. query type): one of "program", "answerset", "atom"
	//	query.input[4] (optional): missing if query type is "program", an answer set index if query type is "answerset", an atom index if query type is "atom"
	// output:
	//      if query type is program: pairs (i, n) for all 0 <= i <= n, where n is the number of answer sets of the program
	//	if query type is answerset: pairs (i, a) for alle atoms with index i in the answer set, and a is the arity of the respective atom
	//	if query type is atom: pairs (0, p) and (i, t[i]) for all 1 <= i <= a, where p is the predicate of the atom, a is its arity and t[i] is the term at argument position i

	const std::vector<InterpretationPtr>& answersets = ctx.getPluginData<NestedHexPlugin>().theNestedHexPlugin->getHexAnswer(ctx, query.input[0], query.input[1], translateInputInterpretation(query.interpretation)).answersets;

	NestedHexPlugin* theNestedHexPlugin = ctx.getPluginData<NestedHexPlugin>().theNestedHexPlugin;

	if (query.input[3] == theNestedHexPlugin->programID){
		if (query.input.size() != 4) throw PluginError("hexInspection with query type \"program\" requires 4 parameters");
		for (int i = 0; i < answersets.size(); ++i){
			Tuple t;
			t.push_back(ID::termFromInteger(i));
			t.push_back(ID::termFromInteger(answersets.size()));
			answer.get().push_back(t);
		}
	}
	else if (query.input[3] == theNestedHexPlugin->answersetID){
		if (query.input.size() != 5) throw PluginError("hexInspection with query type \"answersets\" requires 5 parameters");
		if (!query.input[4].isTerm() || !query.input[4].isIntegerTerm() || query.input[4].address >= answersets.size()) throw PluginError("hexInspection: invalid answer set index");

		DBGLOG(DBG, "Inspecting answer set: " << *answersets[query.input[4].address]);
		bm::bvector<>::enumerator en = answersets[query.input[4].address]->getStorage().first();
		bm::bvector<>::enumerator en_end = answersets[query.input[4].address]->getStorage().end();
		while (en < en_end){
			// do not output auxiliary atoms
			if (!reg->ogatoms.getIDByAddress(*en).isAuxiliary()){
				const OrdinaryAtom& oatom = reg->ogatoms.getByAddress(*en);

				Tuple t;
				t.push_back(ID::termFromInteger(*en));
				t.push_back(ID::termFromInteger(oatom.tuple.size() - 1));
				answer.get().push_back(t);
			}

			en++;
		}
	}
	else if (query.input[3] == theNestedHexPlugin->atomID){
		if (query.input.size() != 5) throw PluginError("hexInspection with query type \"atom\" requires 5 parameters");
		if (!query.input[4].isTerm() || !query.input[4].isIntegerTerm() || query.input[4].address >= reg->ogatoms.getSize()) throw PluginError("hexInspection: invalid atom index");

		const OrdinaryAtom& oatom = reg->ogatoms.getByAddress(query.input[4].address);

		int i = 0;
		BOOST_FOREACH (ID param, oatom.tuple){
			Tuple t;
			t.push_back(ID::termFromInteger(i++));
			t.push_back(param);
			answer.get().push_back(t);
		}
	}
	else{
		throw PluginError("hexInspection was called with invalid query type");
	}
}

void IHEXAtom::answerQuery(PredicateMaskPtr pm, const std::vector<InterpretationPtr>& answersets, const Query& query, Answer& answer){
	assert(false);
}

}

DLVHEX_NAMESPACE_END

/* vim: set noet sw=2 ts=2 tw=80: */

// Local Variables:
// mode: C++
// End:
