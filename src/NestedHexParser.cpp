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
 * @file NestedHexParser.cpp
 * @author Christoph Redl <redl@kr.tuwien.ac.at
 *
 * @brief Implements convenient syntax for nested HEX-programs.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

#include "NestedHexPlugin.h"
#include "ExternalAtoms.h"
#include "NestedHexParser.h"
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

namespace spirit = boost::spirit;
namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

// ============================== Class NestedHexParserModuleSemantics ==============================
// (needs to be in dlvhex namespace)

class NestedHexParserModuleSemantics:
	public HexGrammarSemantics
{
public:
	nestedhex::NestedHexPlugin::CtxData& ctxdata;

	NestedHexParserModuleSemantics(ProgramCtx& ctx):
		HexGrammarSemantics(ctx),
		ctxdata(ctx.getPluginData<nestedhex::NestedHexPlugin>())
	{
	}

	struct nhexAtom:
		SemanticActionBase<NestedHexParserModuleSemantics, ID, nhexAtom>
	{
		nhexAtom(NestedHexParserModuleSemantics& mgr):
			nhexAtom::base_type(mgr)
		{
		}
	};
};

// create semantic handler for semantic action
template<>
struct sem<NestedHexParserModuleSemantics::nhexAtom>
{
	void operator()(
	NestedHexParserModuleSemantics& mgr,
		const boost::fusion::vector5<
			std::string,
			ID,
			boost::optional<std::vector<boost::fusion::vector3<boost::optional<ID>, ID, unsigned int> > >,
			std::vector<ID>,
			boost::optional<std::vector<ID> >
		>& source,
	ID& target)
	{
		static int nextPred = 1;

		DBGLOG(DBG, "Parsing nested HEX-atom with query " << boost::fusion::at_c<0>(source));
		RegistryPtr reg = mgr.ctx.registry();

		std::string type = boost::fusion::at_c<0>(source);
		ID subprogram = boost::fusion::at_c<1>(source);
		std::vector<boost::fusion::vector3<boost::optional<ID>, ID, unsigned int> > emptyin;
		const std::vector<boost::fusion::vector3<boost::optional<ID>, ID, unsigned int> >& in = (!!boost::fusion::at_c<2>(source) ? boost::fusion::at_c<2>(source).get() : emptyin);
		std::vector<ID> query = boost::fusion::at_c<3>(source);
		std::vector<ID> emptyout;
		const std::vector<ID>& out = (!!boost::fusion::at_c<4>(source) ? boost::fusion::at_c<4>(source).get() : emptyout);

		// determine type of query
		ID hexCautiousID = reg->terms.getIDByString("hexCautious");
		ID hexBraveID = reg->terms.getIDByString("hexBrave");
		ID hexInspectionID = reg->terms.getIDByString("hexInspection");

		ExternalAtom ext(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_EXTERNAL);
		ID calltype;
		if (type == "CHEX"){
			ext.predicate = hexCautiousID;
			calltype = reg->storeConstantTerm("string");
			if (query.size() != 1) throw PluginError("CHEX requires queries with one element");
		}else if (type == "BHEX"){
			ext.predicate = hexBraveID;
			calltype = reg->storeConstantTerm("string");
			if (query.size() != 1) throw PluginError("BHEX requires queries with one element");
		}else if (type == "IHEX"){
			ext.predicate = hexInspectionID;
			calltype = reg->storeConstantTerm("string");
			if (query.size() != 1 && query.size() != 2) throw PluginError("IHEX requires queries with one or two elements");
		}else if (type == "CFHEX"){
			ext.predicate = hexCautiousID;
			calltype = reg->storeConstantTerm("file");
			if (query.size() != 1) throw PluginError("CFHEX requires queries with one element");
		}else if (type == "BFHEX"){
			ext.predicate = hexBraveID;
			calltype = reg->storeConstantTerm("file");
			if (query.size() != 1) throw PluginError("CFHEX requires queries with one element");
		}else if (type == "IFHEX"){
			ext.predicate = hexInspectionID;
			calltype = reg->storeConstantTerm("file");
			if (query.size() != 1 && query.size() != 2) throw PluginError("IFHEX requires queries with one or two elements");
		}else{
			assert(false && "invalid nested hex call type");	// this should be avoided by the grammar
		}

		// assemble input to subprogram
		// 1. get new input auxiliary predicate
		ID auxinpPred = reg->getAuxiliaryConstantSymbol('N', ID(0, nextPred++));
		// 2. get maximum arity
		typedef boost::fusion::vector3<boost::optional<ID>, ID, unsigned int> InputPredicate;
		unsigned int maxarity = 0;
		std::vector<ID> vars;
		BOOST_FOREACH (InputPredicate ip, in){
			ID pred = boost::fusion::at_c<1>(ip);
			unsigned int arity = boost::fusion::at_c<2>(ip);
			for (int i = maxarity; i < arity; ++i){
				std::stringstream ss;
				ss << "X" << i;
				vars.push_back(reg->storeVariableTerm(ss.str()));
			}
			if (arity > maxarity) maxarity = arity;
		}
		// 3. create a rule which transfers the elements from the specified input predicates to the auxiliary
		ID emptyID = reg->storeConstantTerm("empty");
		BOOST_FOREACH (InputPredicate ip, in){
			ID pred = boost::fusion::at_c<1>(ip);
			ID mappedpred = (!!boost::fusion::at_c<0>(ip) ? boost::fusion::at_c<0>(ip).get() : pred);
			unsigned int arity = boost::fusion::at_c<2>(ip);

			Rule rule(ID::MAINKIND_RULE);

			OrdinaryAtom auxhead(ID::MAINKIND_ATOM | ID::PROPERTY_AUX);
			OrdinaryAtom bodyatom(ID::MAINKIND_ATOM);

			// for predicate input parameter p/n and max arity m mapped to predicate d, assemble a rule of form
			//    aux(d,n,X1, ..., Xn, empty, empty, ..., empty) :- p(X1, ..., Xn),
			// where the number of empty entries is (m - n)
			if (arity > 0) auxhead.kind |= ID::SUBKIND_ATOM_ORDINARYN; else auxhead.kind |= ID::SUBKIND_ATOM_ORDINARYG;
			if (arity > 0) bodyatom.kind |= ID::SUBKIND_ATOM_ORDINARYN; else bodyatom.kind |= ID::SUBKIND_ATOM_ORDINARYG;
			auxhead.tuple.push_back(auxinpPred);
			auxhead.tuple.push_back(mappedpred);
			auxhead.tuple.push_back(ID::termFromInteger(arity));
			bodyatom.tuple.push_back(pred);
			for (int i = 0; i < arity; ++i){
				auxhead.tuple.push_back(vars[i]);
				bodyatom.tuple.push_back(vars[i]);
			}
			DBGLOG(DBG, "Adding " << (maxarity - arity) << " empty constants");
			for (int i = arity; i < maxarity; ++i) auxhead.tuple.push_back(emptyID);

			rule.head.push_back(reg->storeOrdinaryAtom(auxhead));
			rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(bodyatom)));
			ID ruleID = reg->storeRule(rule);
			mgr.ctx.idb.push_back(ruleID);
#ifndef NDEBUG
			std::string rulestr = RawPrinter::toString(reg, ruleID);
			DBGLOG(DBG, "Created nested hex input rule: " + rulestr);
#endif
		}


		nestedhex::NestedHexPlugin::CtxData& ctxdata = mgr.ctx.getPluginData<nestedhex::NestedHexPlugin>();

		// input is: subprogram name, auxiliary input predicate, query predicate
		ext.inputs.push_back(calltype);
		ext.inputs.push_back(subprogram);
		ext.inputs.push_back(auxinpPred);
		ext.inputs.insert(ext.inputs.end(), query.begin(), query.end());

		// take output terms 1:1
		ext.tuple = out;
		ID extID = reg->eatoms.storeAndGetID(ext);

#ifndef NDEBUG
			std::string extstr = RawPrinter::toString(reg, extID);
			DBGLOG(DBG, "Created external atom for nested hex: " + extstr);
#endif

		target = extID;
	}
};

namespace nestedhex{

namespace
{

template<typename Iterator, typename Skipper>
struct NestedHexParserModuleGrammarBase:
	// we derive from the original hex grammar
	// -> we can reuse its rules
	public HexGrammarBase<Iterator, Skipper>
{
	typedef HexGrammarBase<Iterator, Skipper> Base;

	template<typename Attrib=void, typename Dummy=void>
	struct Rule{
		typedef boost::spirit::qi::rule<Iterator, Attrib(), Skipper> type;
	};
	template<typename Dummy>
	struct Rule<void, Dummy>{
		typedef boost::spirit::qi::rule<Iterator, Skipper> type;
		// BEWARE: this is _not_ the same (!) as
		// typedef boost::spirit::qi::rule<Iterator, boost::spirit::unused_type, Skipper> type;
	};

	typename Rule<std::string>::type nhexName;

	NestedHexParserModuleSemantics& sem;

	qi::rule<Iterator, ID(), Skipper> nhexAtom;

	NestedHexParserModuleGrammarBase(NestedHexParserModuleSemantics& sem):
		Base(sem),
		sem(sem)
	{
		typedef NestedHexParserModuleSemantics Sem;

		nhexName = (qi::string("CHEX"))
			 | (qi::string("BHEX"))
			 | (qi::string("IHEX"))
			 | (qi::string("CFHEX"))
			 | (qi::string("BFHEX"))
			 | (qi::string("IFHEX"));

		nhexAtom
			= (
					nhexName >> qi::lit('[') >> Base::term >> qi::lit(';') >> -((-(Base::pred >> qi::lit('=')) >> Base::pred >> qi::lit('/') >> Base::posinteger) % qi::lit(',')) >> qi::lit(';') >> (Base::term % qi::lit(',')) >> qi::lit(']') >> qi::lit('(') >> -(Base::terms) >> qi::lit(')') > qi::eps
				) [ Sem::nhexAtom(sem) ];

		#ifdef BOOST_SPIRIT_DEBUG
		BOOST_SPIRIT_DEBUG_NODE(nhexAtom);
		#endif
	}
};

struct NestedHexParserModuleGrammar:
  NestedHexParserModuleGrammarBase<HexParserIterator, HexParserSkipper>,
	// required for interface
  // note: HexParserModuleGrammar =
	//       boost::spirit::qi::grammar<HexParserIterator, HexParserSkipper>
	HexParserModuleGrammar
{
	typedef NestedHexParserModuleGrammarBase<HexParserIterator, HexParserSkipper> GrammarBase;
  typedef HexParserModuleGrammar QiBase;

  NestedHexParserModuleGrammar(NestedHexParserModuleSemantics& sem):
    GrammarBase(sem),
    QiBase(GrammarBase::nhexAtom)
  {
  }
};
typedef boost::shared_ptr<NestedHexParserModuleGrammar>
	NestedHexParserModuleGrammarPtr;

template<enum HexParserModule::Type moduletype>
class NestedHexParserModule:
	public HexParserModule
{
public:
	// the semantics manager is stored/owned by this module!
	NestedHexParserModuleSemantics sem;
	// we also keep a shared ptr to the grammar module here
	NestedHexParserModuleGrammarPtr grammarModule;

	NestedHexParserModule(ProgramCtx& ctx):
		HexParserModule(moduletype),
		sem(ctx)
	{
		LOG(INFO,"constructed NestedHexParserModule");
	}

	virtual HexParserModuleGrammarPtr createGrammarModule()
	{
		assert(!grammarModule && "for simplicity (storing only one grammarModule pointer) we currently assume this will be called only once .. should be no problem to extend");
		grammarModule.reset(new NestedHexParserModuleGrammar(sem));
		LOG(INFO,"created NestedHexParserModuleGrammar");
		return grammarModule;
	}
};

} // anonymous namespace

void NestedHexParser::createParserModule(std::vector<HexParserModulePtr>& ret, ProgramCtx& ctx){
	ret.push_back(HexParserModulePtr(new NestedHexParserModule<HexParserModule::BODYATOM>(ctx)));
}

}

DLVHEX_NAMESPACE_END

/* vim: set noet sw=2 ts=2 tw=80: */

// Local Variables:
// mode: C++
// End:
