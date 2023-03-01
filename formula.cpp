// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "formula.h"
#include "antlr4_runtime/runtime/src/antlr4-runtime.h"

#include <FormulaListener.h>
#include <FormulaLexer.h>
#include <sstream>
#include <utility>
using namespace std;

class MyListener : public FormulaListener {
  public:
    virtual void enterMain(FormulaParser::MainContext *ctx) override {};
    virtual void exitMain(FormulaParser::MainContext *ctx) override {}

    virtual void enterUnaryOp(FormulaParser::UnaryOpContext *ctx) override {}
    virtual void exitUnaryOp(FormulaParser::UnaryOpContext *ctx) override {
        stack_of_tokens_.push_back(
        parsedToken {
            parsedTokenType::unary_operator,
            ctx->getToken(FormulaParser::ADD, 0) == nullptr ? ctx->SUB()->getText() : ctx->ADD()->getText()
        }
        );
    }

    virtual void enterParens(FormulaParser::ParensContext *ctx) override {}
    virtual void exitParens(FormulaParser::ParensContext *ctx) override {
        stack_of_tokens_.push_back(
        parsedToken {
            parsedTokenType::parens,
            ctx->getText()
        }
        );
    }

    virtual void enterLiteral(FormulaParser::LiteralContext *ctx) override {}
    virtual void exitLiteral(FormulaParser::LiteralContext *ctx) override {
        stack_of_tokens_.push_back(
        parsedToken {
            parsedTokenType::number,
            ctx->NUMBER()->getText()
        }
        );
    }

    virtual void enterCell(FormulaParser::CellContext *ctx) override {}
    virtual void exitCell(FormulaParser::CellContext *ctx) override {
        stack_of_tokens_.push_back(
        parsedToken {
            parsedTokenType::cell,
            ctx->CELL()->getText()
        }
        );
    }

    virtual void enterBinaryOp(FormulaParser::BinaryOpContext *ctx) override {}
    virtual void exitBinaryOp(FormulaParser::BinaryOpContext *ctx) override {
        string data;
        if (ctx->getToken(FormulaParser::ADD, 0) != nullptr) {
            stack_of_tokens_.push_back(
            parsedToken {
                parsedTokenType::add,
                ctx->ADD()->getText()
            }
            );
        } else if (ctx->getToken(FormulaParser::SUB, 0) != nullptr) {
            stack_of_tokens_.push_back(
            parsedToken {
                parsedTokenType::sub,
                ctx->SUB()->getText()
            }
            );
        } else if (ctx->getToken(FormulaParser::MUL, 0) != nullptr) {
            stack_of_tokens_.push_back(
            parsedToken {
                parsedTokenType::mul,
                ctx->MUL()->getText()
            }
            );
        } else if (ctx->getToken(FormulaParser::DIV, 0) != nullptr) {
            stack_of_tokens_.push_back(
            parsedToken {
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

    IFormula::Value GetResult() {
        auto result = recursiveComputations(stack_of_tokens_.crbegin());
        if (result.second == stack_of_tokens_.crend() || holds_alternative<FormulaError>(result.first))
            return result.first;
        else
            throw runtime_error("shit happened during formula coputing 11111111");
    }
  private:
    enum class parsedTokenType {
        unary_operator,
        add,
        sub,
        mul,
        div,
        cell,
        parens,
        number
    };
    struct parsedToken {
        parsedTokenType type;
        string data;
    };
    vector<parsedToken> stack_of_tokens_;
    pair<IFormula::Value, vector<parsedToken>::const_reverse_iterator> recursiveComputations(vector<parsedToken>::const_reverse_iterator current_root_token) {
        switch (current_root_token->type) {
        case parsedTokenType::parens:
            return recursiveComputations(next(current_root_token));
        case parsedTokenType::number:
            return {double(stoi(current_root_token->data)), next(current_root_token)};
        case parsedTokenType::cell:
            cout << "CELL WAS INTERPRETED AS ZERO. TODO" << endl;
            return {0.0, next(current_root_token)};
        case parsedTokenType::unary_operator:
            if (current_root_token->data == "+")
                return recursiveComputations(next(current_root_token));
            else {
                auto result = recursiveComputations(next(current_root_token));
                if (holds_alternative<FormulaError>(result.first))
                    return result;
                result.first = - get<double>(result.first);
                return result;
            }
        default:
            auto right_part_result = recursiveComputations(next(current_root_token));
            if (holds_alternative<FormulaError>(right_part_result.first))
                return right_part_result;
            auto left_part_result = recursiveComputations(right_part_result.second);
            if (holds_alternative<FormulaError>(left_part_result.first))
                return right_part_result;
            double left_val = get<double>(left_part_result.first);
            double right_val = get<double>(right_part_result.first);
            switch (current_root_token->type) {
            case parsedTokenType::add :
                return {left_val + right_val, left_part_result.second};
            case parsedTokenType::sub :
                return {left_val - right_val, left_part_result.second};
            case parsedTokenType::mul :
                return {left_val * right_val, left_part_result.second};
            case parsedTokenType::div :
                return right_val == 0 ?
                       pair<IFormula::Value, vector<parsedToken>::const_reverse_iterator> {FormulaError(FormulaError::Category::Div0), left_part_result.second} :
                       pair<IFormula::Value, vector<parsedToken>::const_reverse_iterator> {left_val / right_val, left_part_result.second};
            default:
                throw runtime_error("shit happened during formula coputing 22222222");
            }
        }
    }
};

class BailErrorListener : public antlr4::BaseErrorListener {
  public:
    void syntaxError(antlr4::Recognizer* /* recognizer */,
                     antlr4::Token* /* offendingSymbol */, size_t /* line */,
                     size_t /* charPositionInLine */, const string& msg,
                     exception_ptr /* e */
                    ) override {
        throw runtime_error("Error when lexing: " + msg);
    }
};

IFormula::Value ParseCell(istream& in) {
    antlr4::ANTLRInputStream input(in);

    FormulaLexer lexer(&input);
    BailErrorListener error_listener;
    lexer.removeErrorListeners();
    lexer.addErrorListener(&error_listener);

    antlr4::CommonTokenStream tokens(&lexer);

    FormulaParser parser(&tokens);
    auto error_handler = make_shared<antlr4::BailErrorStrategy>();
    parser.setErrorHandler(error_handler);
    parser.removeErrorListeners();

    antlr4::tree::ParseTree* tree = parser.main();  // метод соответствует корневому правилу
    MyListener listener;
    antlr4::tree::ParseTreeWalker::DEFAULT.walk(&listener, tree);

    return listener.GetResult();
}

class Formula: public IFormula {
  public:
    virtual ~Formula() = default;
    Formula() = default;
    Formula(string expression) : expression_(expression) {}
    virtual Value Evaluate(const ISheet& sheet) const override {
        istringstream streamed(expression_);
        return ParseCell(streamed);
    }
    virtual string GetExpression() const override {
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

unique_ptr<IFormula> ParseFormula(string expression) {
    return unique_ptr<IFormula>(make_unique<Formula>(expression));
}
