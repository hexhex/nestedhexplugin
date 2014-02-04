
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
 * @file NestedHexPlugin.h
 * @author Christoph Redl
 *
 * @brief Supports queries to subprograms.
 */

#ifndef NESTEDHEX_PLUGIN__HPP_INCLUDED_
#define NESTEDHEX_PLUGIN__HPP_INCLUDED_

#include "dlvhex2/PlatformDefinitions.h"
#include "dlvhex2/PluginInterface.h"
#include "dlvhex2/ComponentGraph.h"
#include "dlvhex2/HexGrammar.h"
#include "dlvhex2/HexParserModule.h"
#include "dlvhex2/ProgramCtx.h"
#include <set>


DLVHEX_NAMESPACE_BEGIN

class NestedHexPlugin:
  public PluginInterface
{
public:
	struct HexAnswer{
		ProgramCtx pc;
		ID type;
		ID program;
		InterpretationPtr input;
		std::vector<InterpretationPtr> answersets;
	};

	class CtxData : public PluginData
	{
	public:
		std::vector<HexAnswer> cache;

		NestedHexPlugin* theNestedHexPlugin;
		bool rewrite;	// automatically rewrite HEX-atoms?
		CtxData() : rewrite(false) {};
		virtual ~CtxData() {};
	};

private:
	// base class for all DL atoms
	class NestedHexPluginAtom : public PluginAtom{
	protected:
		ProgramCtx& ctx;
		RegistryPtr reg;

		bool positivesubprogram;

		InterpretationPtr translateInputInterpretation(InterpretationConstPtr input);
	public:
		NestedHexPluginAtom(std::string predName, ProgramCtx& ctx, bool positivesubprogram = false);

		virtual void retrieve(const Query& query, Answer& answer);
		virtual void retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods);
		virtual void learnSupportSets(const Query& query, NogoodContainerPtr nogoods);

 		// define an abstract method for aggregating the answer sets (this part is specific for cautious and brave queries)
		virtual void answerQuery(PredicateMaskPtr pm, const std::vector<InterpretationPtr>& answersets, const Query& query, Answer& answer) = 0;
	};

	// cautious queries
	class CHEXAtom : public NestedHexPluginAtom{
	public:
		CHEXAtom(ProgramCtx& ctx);
		virtual void answerQuery(PredicateMaskPtr pm, const std::vector<InterpretationPtr>& answersets, const Query& query, Answer& answer);
	};

	// brave queries
	class BHEXAtom : public NestedHexPluginAtom{
	public:
		BHEXAtom(ProgramCtx& ctx);
		virtual void answerQuery(PredicateMaskPtr pm, const std::vector<InterpretationPtr>& answersets, const Query& query, Answer& answer);
	};

	// inspection of hex program answers
	class IHEXAtom : public NestedHexPluginAtom{
	public:
		IHEXAtom(ProgramCtx& ctx);
		virtual void retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods);
		virtual void answerQuery(PredicateMaskPtr pm, const std::vector<InterpretationPtr>& answersets, const Query& query, Answer& answer);
	};

	RegistryPtr reg;

	// initializes the frequently used IDs
	void prepareIDs();
protected:
	ID fileID, stringID, programID, answersetID, atomID, emptyID;

	const HexAnswer& getHexAnswer(ProgramCtx& ctx, ID type, ID program, InterpretationPtr input);

public:
	NestedHexPlugin();
	virtual ~NestedHexPlugin();

	virtual void processOptions(std::list<const char*>& pluginOptions, ProgramCtx& ctx);

	// create parser modules that extend and the basic hex grammar
	virtual std::vector<HexParserModulePtr> createParserModules(ProgramCtx&);

	virtual void printUsage(std::ostream& o) const;

	// plugin atoms
	virtual std::vector<PluginAtomPtr> createAtoms(ProgramCtx& ctx) const;

	virtual void setRegistry(RegistryPtr reg);
	virtual void setupProgramCtx(ProgramCtx& ctx);
};

DLVHEX_NAMESPACE_END

#endif
