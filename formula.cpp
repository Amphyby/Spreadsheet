// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "formula.h"
#include "antlr4_runtime/runtime/src/antlr4-runtime.h"

#include <FormulaListener.h>
#include <FormulaLexer.h>
#include <sstream>
using namespace std;

class MyListener : public FormulaListener
{
public:
    virtual void enterMain(FormulaParser::MainContext *ctx) override {};
    virtual void exitMain(FormulaParser::MainContext *ctx) override {}

    virtual void enterUnaryOp(FormulaParser::UnaryOpContext *ctx) override {}
    virtual void exitUnaryOp(FormulaParser::UnaryOpContext *ctx) override
    {
        stack_of_tokens.push_back(
            parsedToken
            {
            parsedTokenType::unary_operator,
            ctx->getToken(FormulaParser::ADD, 0) == nullptr ? ctx->SUB()->getText() : ctx->ADD()->getText()
            }
        );
    }

    virtual void enterParens(FormulaParser::ParensContext *ctx) override {}
    virtual void exitParens(FormulaParser::ParensContext *ctx) override
    {
        stack_of_tokens.push_back(
            parsedToken
            {
            parsedTokenType::parens,
            ctx->getText()
            }
        );
    }

    virtual void enterLiteral(FormulaParser::LiteralContext *ctx) override {}
    virtual void exitLiteral(FormulaParser::LiteralContext *ctx) override
    {
        stack_of_tokens.push_back(
            parsedToken
            {
            parsedTokenType::number,
            ctx->NUMBER()->getText()
            }
        );
    }

    virtual void enterCell(FormulaParser::CellContext *ctx) override {}
    virtual void exitCell(FormulaParser::CellContext *ctx) override
    {
        stack_of_tokens.push_back(
            parsedToken
            {
            parsedTokenType::cell,
            ctx->CELL()->getText()
            }
        );
    }

    virtual void enterBinaryOp(FormulaParser::BinaryOpContext *ctx) override {}
    virtual void exitBinaryOp(FormulaParser::BinaryOpContext *ctx) override
    {
        std::string data;
        if (ctx->getToken(FormulaParser::ADD, 0) != nullptr)
            {
            stack_of_tokens.push_back(
                parsedToken
                {
                parsedTokenType::add,
                ctx->ADD()->getText()
                }
            );
            }
        else if (ctx->getToken(FormulaParser::SUB, 0) != nullptr)
            {
            stack_of_tokens.push_back(
                parsedToken
                {
                parsedTokenType::sub,
                ctx->SUB()->getText()
                }
            );
            }
        else if (ctx->getToken(FormulaParser::MUL, 0) != nullptr)
            {
            stack_of_tokens.push_back(
                parsedToken
                {
                parsedTokenType::mul,
                ctx->MUL()->getText()
                }
            );
            }
        else if (ctx->getToken(FormulaParser::DIV, 0) != nullptr)
            {
            stack_of_tokens.push_back(
                parsedToken
                {
                parsedTokenType::div,
                ctx->DIV()->getText()
                }
            );
            }
    }

    virtual void visitTerminal(antlr4::tree::TerminalNode *node) override {}
    virtual void visitErrorNode(antlr4::tree::ErrorNode *node) override {}
    virtual void enterEveryRule(antlr4::ParserRuleContext *ctx) override {}
    virtual void exitEveryRule(antlr4::ParserRuleContext *ctx) override {}

    IFormula::Value GetResult()
    {
        return FormulaError(FormulaError::Category::Value);
    }
private:
    enum class parsedTokenType
    {
        unary_operator,
        add,
        sub,
        mul,
        div,
        cell,
        parens,
        number
    };
    struct parsedToken
    {
        parsedTokenType type;
        std::string data;
    };
    std::vector<parsedToken> stack_of_tokens;
};

class BailErrorListener : public antlr4::BaseErrorListener
{
public:
    void syntaxError(antlr4::Recognizer* /* recognizer */,
                     antlr4::Token* /* offendingSymbol */, size_t /* line */,
                     size_t /* charPositionInLine */, const std::string& msg,
                     std::exception_ptr /* e */
                    ) override
    {
        throw std::runtime_error("Error when lexing: " + msg);
    }
};

IFormula::Value ParseCell(std::istream& in)
{
    antlr4::ANTLRInputStream input(in);

    FormulaLexer lexer(&input);
    BailErrorListener error_listener;
    lexer.removeErrorListeners();
    lexer.addErrorListener(&error_listener);

    antlr4::CommonTokenStream tokens(&lexer);

    FormulaParser parser(&tokens);
    auto error_handler = std::make_shared<antlr4::BailErrorStrategy>();
    parser.setErrorHandler(error_handler);
    parser.removeErrorListeners();

    antlr4::tree::ParseTree* tree = parser.main();  // метод соответствует корневому правилу
    MyListener listener;
    antlr4::tree::ParseTreeWalker::DEFAULT.walk(&listener, tree);

    return listener.GetResult();
}

class Formula: public IFormula
{
public:
    virtual ~Formula() = default;
    Formula() = default;
    Formula(string expression) : expression_(expression) {}
    virtual Value Evaluate(const ISheet& sheet) const override
    {
        istringstream streamed(expression_);
        return ParseCell(streamed);
    }
    virtual string GetExpression() const override
    {
        return expression_;
    }
    virtual vector<Position> GetReferencedCells() const override {}
    virtual HandlingResult HandleInsertedRows(int first, int count = 1) override {}
    virtual HandlingResult HandleInsertedCols(int first, int count = 1) override {}
    virtual HandlingResult HandleDeletedRows(int first, int count = 1) override {}
    virtual HandlingResult HandleDeletedCols(int first, int count = 1) override {}
private:
    string expression_;
};

unique_ptr<IFormula> ParseFormula(string expression)
{
    return unique_ptr<IFormula>(make_unique<Formula>(expression));
}
