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
 * @file NestedHexPlugin.cpp
 * @author Christoph Redl
 *
 * @brief Supports queries to subprograms.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

#include "NestedHexPlugin.h"
#include "ExternalAtoms.h"
#include "Parser.h"
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

#ifndef NDEBUG
	#define CheckPredefinedIDs ((fileID != ID_FAIL && stringID != ID_FAIL && programID != ID_FAIL && answersetID != ID_FAIL && atomID != ID_FAIL && emptyID != ID_FAIL))
#endif

dlvhex::nestedhex::NestedHexPlugin theNestedHexPlugin;

// ============================== Class NestedHexPlugin ==============================

void NestedHexPlugin::prepareIDs(){

	assert(!!reg && "registry must be set before IDs can be prepared");

	DBGLOG(DBG, "Prepare IDs " << this);

#ifndef NDEBUG
	if (CheckPredefinedIDs){
		DBGLOG(DBG, "IDs have already been prepared");
		return;
	}
#endif

	// prepare some frequently used terms and atoms
	fileID = reg->storeConstantTerm("file");
	stringID = reg->storeConstantTerm("string");
	programID = reg->storeConstantTerm("program");
	answersetID = reg->storeConstantTerm("answerset");
	atomID = reg->storeConstantTerm("atom");
	emptyID = reg->storeConstantTerm("empty");
}

const NestedHexPlugin::HexAnswer& NestedHexPlugin::getHexAnswer(ProgramCtx& ctx, ID type, ID program, InterpretationPtr input){

	assert(CheckPredefinedIDs && "IDs have not been initialized");
	assert(!!input && "invalid input interpretation");

	DBGLOG(DBG, "Checking if answer is in cache");
	int cacheSize = ctx.getPluginData<NestedHexPlugin>().cache.size();
	for (int i = 0; i < cacheSize; ++i){
		HexAnswer& answer = ctx.getPluginData<NestedHexPlugin>().cache[i];
		assert(!!answer.input && "Invalid cache entry");
		if ((answer.type == type) && (answer.program == program) && (answer.input->getStorage() == input->getStorage())){
			DBGLOG(DBG, "Retrieving answer sets from cache");
			return answer;
		}
	}

	DBGLOG(DBG, "Answer was not found in cache");

	// read the subprogram from the file
	InputProviderPtr ip(new InputProvider());
	if (type == fileID) ip->addFileInput(ctx.registry()->terms.getByID(program).getUnquotedString());
	else if (type == stringID) ip->addStringInput(ctx.registry()->terms.getByID(program).getUnquotedString(), "subprogram");
	else { assert(false && "invalid call type"); }

	// not in cache --> add it
	ctx.getPluginData<NestedHexPlugin>().cache.push_back(HexAnswer());
	HexAnswer& answer = ctx.getPluginData<NestedHexPlugin>().cache[ctx.getPluginData<NestedHexPlugin>().cache.size() - 1];
	answer.type = type;
	answer.program = program;
	answer.input = input;

	// prepare data structures for the subprogram P
	answer.pc = ctx;
	answer.pc.idb.clear();
	answer.pc.edb = input;
	answer.pc.currentOptimum.clear();
	answer.pc.config.setOption("NumberOfModels",0);
	answer.pc.inputProvider = ip;
	ip.reset();

	// compute all answer sets of P \cup F
	try{
		DBGLOG(DBG, "Evaluating subprogram under " << *input);
		answer.answersets = ctx.evaluateSubprogram(answer.pc, true);
	}catch(...){
		throw PluginError("Error during evaluation of subprogram " + RawPrinter::toString(reg, answer.program));
	}	

	return answer;
}

// Collect all types of external atoms 
NestedHexPlugin::NestedHexPlugin():
	PluginInterface()
{
	DBGLOG(DBG, "NestedHexPlugin constructor");
	setNameVersion(PACKAGE_TARNAME,NESTEDHEXPLUGIN_VERSION_MAJOR,NESTEDHEXPLUGIN_VERSION_MINOR,NESTEDHEXPLUGIN_VERSION_MICRO);
}

NestedHexPlugin::~NestedHexPlugin()
{
}

// Define two external atoms: for the roles and for the concept queries
std::vector<PluginAtomPtr> NestedHexPlugin::createAtoms(ProgramCtx& ctx) const{
	std::vector<PluginAtomPtr> ret;
	ret.push_back(PluginAtomPtr(new CHEXAtom(ctx), PluginPtrDeleter<PluginAtom>()));
	ret.push_back(PluginAtomPtr(new BHEXAtom(ctx), PluginPtrDeleter<PluginAtom>()));
	ret.push_back(PluginAtomPtr(new IHEXAtom(ctx), PluginPtrDeleter<PluginAtom>()));
	return ret;
}

void NestedHexPlugin::processOptions(std::list<const char*>& pluginOptions, ProgramCtx& ctx){

	std::vector<std::list<const char*>::iterator> found;
	for(std::list<const char*>::iterator it = pluginOptions.begin(); it != pluginOptions.end(); it++){
		std::string option(*it);
		if (option.find("--nestedhex") != std::string::npos){
			ctx.getPluginData<NestedHexPlugin>().rewrite = true;
			found.push_back(it);
		}
	}

	for(std::vector<std::list<const char*>::iterator>::const_iterator it = found.begin(); it != found.end(); ++it){
		pluginOptions.erase(*it);
	}
}

// create parser modules that extend and the basic hex grammar
// this parser also stores the query information into the plugin
std::vector<HexParserModulePtr>
NestedHexPlugin::createParserModules(ProgramCtx& ctx)
{
	DBGLOG(DBG,"NestedHexPlugin::createParserModules(ProgramCtx& ctx)");
	if (!!this->reg){
		DBGLOG(DBG, "Registry was already previously set");
		assert(this->reg == ctx.registry() && "NestedHexPlugin: registry pointer passed in ctx.registry() to createParserModules(ProgramCtx& ctx) is different from previously set one, do not know what to do");
	}
	this->reg = ctx.registry();

	std::vector<HexParserModulePtr> ret;

	prepareIDs();
	NestedHexPlugin::CtxData& ctxdata = ctx.getPluginData<NestedHexPlugin>();
	if (ctxdata.rewrite){
		// the parser needs the registry, so make sure that the pointer is set
		DBGLOG(DBG,"rewriting is enabled");
		Parser::createParserModule(ret, ctx);
	}

	ctx.getPluginData<NestedHexPlugin>().theNestedHexPlugin = this;

	return ret;
}

void NestedHexPlugin::printUsage(std::ostream& o) const{
	o << "     --nestedhex                 Activates convenient syntax for queries over nested hex programs" << std::endl <<
	     "" << std::endl <<
	     "     The plugin supports the following external atoms:" << std::endl <<
	     "" << std::endl <<
	     "     - &hexCautious[t, p, i, q](x1, ..., xn) / &hexBrave[t, p, i, q](x1, ..., xn)" << std::endl <<
	     "" << std::endl <<
	     "          Evaluates the program specified by p (which must be a filename" << std::endl <<
	     "          if type t=file and the a string containing the rules if t=string)." << std::endl <<
	     "" << std::endl <<
	     "          Prior to evaluation, p is extended by facts specified in higher-order notation" << std::endl <<
	     "          by input predicate i. The arity of i is the maximum arity m of all facts to be added + 2." << std::endl <<
	     "          For adding a fact of form f(c1, ..., ck), predicate i is suppose to specify" << std::endl <<
	     "          element i(f, m, c1, ... ck, empty, ..., empty)," << std::endl <<
	     "          where the number of terms of form empty is m - k, i.e.," << std::endl <<
             "          the empty terms fill additional argument positions in i which are not needed for a certain fact" << std::endl <<
	     "          due to smaller arity." << std::endl <<
	     "" << std::endl <<
	     "          Parameter q specifies the query predicate of arity n." << std::endl <<
	     "" << std::endl <<
	     "          The external atom evaluates to true for all values x1, ..., xn" << std::endl <<
	     "          such that q(x1, ..., xn) is cautiously/bravely true in p extended with the input from i." << std::endl <<
	     "" << std::endl <<
	     "     - &hexInspection[t, p, i, qt, qp](x1, x2)" << std::endl <<
	     "" << std::endl <<
	     "          Evaluates the program p of type t extended with input from i as described above." << std::endl <<
	     "          Parameter qp is optional." << std::endl <<
	     "" << std::endl <<
	     "          If qt=program and qp is missing, then the external atom is true for all pairs (x, n) with 0 <= x <= n," << std::endl <<
	     "          where n is the number of answer sets of the program. Elements x are intended to identify answer sets." << std::endl <<
	     "" << std::endl <<
	     "          If qt=answerset and qp is an integer, then the external atom is true for all pairs (x, a)" << std::endl <<
	     "          which encode the true atoms in the answer set identified by qp. A pair (x, a) consists of" << std::endl <<
	     "          an integer identifier x for this atom and its arity a." << std::endl <<
	     "" << std::endl <<
	     "          If qt=atom and qp is an integer, then the external atom is true for all pairs (x, t)" << std::endl <<
	     "          which encode the atom identified by qp. If the identified atom has arity a, then pairs (x, t)" << std::endl <<
	     "          for 0 <= x <= a consist of encode the term t at argument position x, where x=0 denotes the predicate name." << std::endl <<
	     "" << std::endl <<
	     "     The command-line option --nestedhex activates a rewriter, which allows for using a more convenient syntax" << std::endl <<
             "          (for details see http://www.kr.tuwien.ac.at/research/systems/dlvhex/nestedhexplugin.html)" << std::endl;


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


}

void NestedHexPlugin::setRegistry(RegistryPtr reg){

	DBGLOG(DBG,"NestedHexPlugin::setRegistry(RegistryPtr reg)");
	if (!!this->reg){
		DBGLOG(DBG, "Registry was already previously set");
		assert(this->reg == reg && "NestedHexPlugin: registry pointer passed to setRegistry(RegistryPtr) is different from previously set one, do not know what to do");
	}
	this->reg = reg;
	prepareIDs();
}

void NestedHexPlugin::setupProgramCtx(ProgramCtx& ctx){

	DBGLOG(DBG,"NestedHexPlugin::setupProgramCtx(ProgramCtx& ctx)");
	if (!!reg){
		DBGLOG(DBG, "Registry was already previously set");
		assert(this->reg == ctx.registry() && "NestedHexPlugin: registry pointer passed in ctx.registry() to setupProgramCtx(ProgramCtx& ctx) is different from previously set one, do not know what to do");
	}
	this->reg = ctx.registry();
	prepareIDs();
}

}

DLVHEX_NAMESPACE_END

IMPLEMENT_PLUGINABIVERSIONFUNCTION

// return plain C type s.t. all compilers and linkers will like this code
extern "C"
void * PLUGINIMPORTFUNCTION()
{
	return reinterpret_cast<void*>(& dlvhex::nestedhex::theNestedHexPlugin);
}

/* vim: set noet sw=2 ts=2 tw=80: */

// Local Variables:
// mode: C++
// End:
