/*
	Shader Parser class

	Parses metadata:
	Constant buffer declarations,
	Resource declarations (textures/samplers)
	Functions (including parameters/return types)
*/

#pragma once

#include <vector>
#include <set>
#include <istream>
#include <tscore/strings.h>
#include <tscore/types.h>

#include "ShaderAnnotation.h"
#include "ShaderType.h"

namespace ts
{
	class Scanner;

	class ShaderParser
	{
	public:

		struct Register
		{
			char prefix;
			uint16 slot;

			Register() : slot(0), prefix(0) {}
		};

		struct ConstantsDeclaration
		{
			Register reg;
			String name;
			std::vector<StructMember> members;

			ShaderAnnotationSet annotations;

			ConstantsDeclaration() {}
			ConstantsDeclaration(const String& name) : name(name) {}

			bool operator==(const ConstantsDeclaration& decl) const { return this->name == decl.name; }
			bool operator<(const ConstantsDeclaration& decl) const { return this->name < decl.name; }
		};

		struct ResourceDeclaration
		{
			Register reg;
			String type;
			String name;

			ResourceDeclaration() {}
			ResourceDeclaration(const String& name) : name(name) {}

			bool operator==(const ResourceDeclaration& decl) const { return this->name == decl.name; }
			bool operator<(const ResourceDeclaration& decl) const { return this->name < decl.name; }
		};

		struct FunctionDeclaration
		{
			const MemberType* returnType;
			String returnSemantic;
			std::vector<StructMember> parameters;
			String name;

			ShaderAnnotationSet annotations;

			FunctionDeclaration() : returnType(nullptr) {}
			FunctionDeclaration(const String& name) : name(name) {}

			bool operator==(const FunctionDeclaration& decl) const { return this->name == decl.name; }
			bool operator<(const FunctionDeclaration& decl) const { return this->name < decl.name; }
		};

		/*
			Parse a sequence of shader code
		*/
		ShaderParser(std::istream& stream)
		{
			parse(stream);
		}

		bool parse(std::istream& stream);

		ShaderParser() {}

		using ConstantDeclSet = std::set<ConstantsDeclaration>;
		using FunctionDeclSet = std::set<FunctionDeclaration>;
		using ResourceDeclSet = std::set<ResourceDeclaration>;

		/*
			Access Shader Function information
		*/
		bool isFunction(const String& name) const;
		FunctionDeclaration getFunction(const String& name) const;
		const FunctionDeclSet& functions() const { return m_functions; }

		/*
			Access Shader Resource information
		*/
		bool isResource(const String& name) const;
		ResourceDeclaration getResource(const String& name) const;
		const ResourceDeclSet& resources() const { return m_resources; }

		/*
			Access Shader Constant buffer information
		*/
		bool isConstantBuffer(const String& name) const;
		ConstantsDeclaration getConstantBuffer(const String& name) const;
		const ConstantDeclSet& beginConstants() const { return m_constants; }

	private:

		std::unique_ptr<TypeContext> m_types;

		ConstantDeclSet m_constants;
		ResourceDeclSet m_resources;
		FunctionDeclSet m_functions;

		Register parseRegister(Scanner& scanner, const String& prefixes);
		void parseAnnotations(Scanner& scanner, ShaderAnnotationSet& annotations);
		void parseFunctionParameters(Scanner& scanner, std::vector<StructMember>& parameters);
		void parseFunctionDeclaration(Scanner& scanner);
		void parseStructMember(Scanner& scanner, StructMemberInfo& member);
		void parseStruct(Scanner& scanner);
		void parseResourceDeclaration(Scanner& scanner);
		void parseConstantsDeclaration(Scanner& scanner);

		bool isBasicType(const String& identifier) const;
		bool isStructType(const String& identifier) const;
		bool isType(const String& identifier) const;
		bool isResourceType(const String& identifier) const;
	};
}
