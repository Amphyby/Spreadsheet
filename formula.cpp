// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "formula.h"
#include "antlr4_runtime/runtime/src/antlr4-runtime.h"

#include <FormulaListener.h>
#include <FormulaLexer.h>
#include <sstream>
#include <utility>
#include <set>
using namespace std;

class MyListener : public FormulaListener {
  public:
    /*class CacheSingletone {
      public:
        map<string, IFormula::Value> cache_of_cell_values_;
        CacheSingletone() = default;
        static CacheSingletone& getInstance() {
            if (instance_ == nullptr)
                instance_ = make_unique<CacheSingletone>();
        }
      private:
        static unique_ptr<CacheSingletone> instance_;
    };*/
    MyListener(const ISheet& sheet) : sheet_(sheet) {}

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
        case parsedTokenType::cell: {
            Position pos = Position::FromString(current_root_token->data);
            const ICell* cell = sheet_.GetCell(pos);
            if (cell == nullptr) {
                return { 0.0, next(current_root_token) };
            }
            auto cell_value = cell->GetValue();
            if (holds_alternative<string>(cell_value))
                return {FormulaError(FormulaError::Category::Value), next(current_root_token)};
            if (holds_alternative<FormulaError>(cell_value))
                return {get<FormulaError>(cell_value), next(current_root_token)};
            return {get<double>(cell_value), next(current_root_token)};
        }
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
                return left_part_result;
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
                throw runtime_error("shit happened during formula computing 22222222");
            }
        }
    }
    const ISheet& sheet_;
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

class ReformattingListener : public FormulaListener {
public:
    /*class CacheSingletone {
      public:
        map<string, IFormula::Value> cache_of_cell_values_;
        CacheSingletone() = default;
        static CacheSingletone& getInstance() {
            if (instance_ == nullptr)
                instance_ = make_unique<CacheSingletone>();
        }
      private:
        static unique_ptr<CacheSingletone> instance_;
    };*/
    ReformattingListener() = default;

    virtual void enterMain(FormulaParser::MainContext* ctx) override {};
    virtual void exitMain(FormulaParser::MainContext* ctx) override {}

    virtual void enterUnaryOp(FormulaParser::UnaryOpContext* ctx) override {}
    virtual void exitUnaryOp(FormulaParser::UnaryOpContext* ctx) override {
        stack_of_tokens_.push_back(
            parsedToken{
                parsedTokenType::unary_operator,
                ctx->getToken(FormulaParser::ADD, 0) == nullptr ? ctx->SUB()->getText() : ctx->ADD()->getText()
            }
        );
    }

    virtual void enterParens(FormulaParser::ParensContext* ctx) override {}
    virtual void exitParens(FormulaParser::ParensContext* ctx) override {
        stack_of_tokens_.push_back(
            parsedToken{
                parsedTokenType::parens,
                ctx->getText()
            }
        );
    }

    virtual void enterLiteral(FormulaParser::LiteralContext* ctx) override {}
    virtual void exitLiteral(FormulaParser::LiteralContext* ctx) override {
        stack_of_tokens_.push_back(
            parsedToken{
                parsedTokenType::number,
                ctx->NUMBER()->getText()
            }
        );
    }

    virtual void enterCell(FormulaParser::CellContext* ctx) override {}
    virtual void exitCell(FormulaParser::CellContext* ctx) override {
        stack_of_tokens_.push_back(
            parsedToken{
                parsedTokenType::cell,
                ctx->CELL()->getText()
            }
        );
    }

    virtual void enterBinaryOp(FormulaParser::BinaryOpContext* ctx) override {}
    virtual void exitBinaryOp(FormulaParser::BinaryOpContext* ctx) override {
        string data;
        if (ctx->getToken(FormulaParser::ADD, 0) != nullptr) {
            stack_of_tokens_.push_back(
                parsedToken{
                    parsedTokenType::add,
                    ctx->ADD()->getText()
                }
            );
        }
        else if (ctx->getToken(FormulaParser::SUB, 0) != nullptr) {
            stack_of_tokens_.push_back(
                parsedToken{
                    parsedTokenType::sub,
                    ctx->SUB()->getText()
                }
            );
        }
        else if (ctx->getToken(FormulaParser::MUL, 0) != nullptr) {
            stack_of_tokens_.push_back(
                parsedToken{
                    parsedTokenType::mul,
                    ctx->MUL()->getText()
                }
            );
        }
        else if (ctx->getToken(FormulaParser::DIV, 0) != nullptr) {
            stack_of_tokens_.push_back(
                parsedToken{
                    parsedTokenType::div,
                    ctx->DIV()->getText()
                }
            );
        }
    }

    virtual void visitTerminal(antlr4::tree::TerminalNode* node) override {}
    virtual void visitErrorNode(antlr4::tree::ErrorNode* node) override {}
    virtual void enterEveryRule(antlr4::ParserRuleContext* ctx) override {}
    virtual void exitEveryRule(antlr4::ParserRuleContext* ctx) override {}

    string GetResult() {
        return printStack(stack_of_tokens_.crbegin()).first;
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
    pair<string, vector<parsedToken>::const_reverse_iterator> printStack(vector<parsedToken>::const_reverse_iterator current_root_token) {
        switch (current_root_token->type) {
        case parsedTokenType::parens:
            return printStack(next(current_root_token));
        case parsedTokenType::number:
        case parsedTokenType::cell:
            return { current_root_token->data,  next(current_root_token) };
        case parsedTokenType::unary_operator: {
            auto temp_result = printStack(next(current_root_token));
            if (next(current_root_token)->type == parsedTokenType::parens)//TODO: возможно, для + не нужно
                return { current_root_token->data + "(" + temp_result.first + ")", temp_result.second };
            else
                return { current_root_token->data + temp_result.first, temp_result.second };
            break;
        }
        default:
            auto right_subtree = printStack(next(current_root_token));
            auto left_subtree = printStack(right_subtree.second);
            switch (current_root_token->type) {
            case parsedTokenType::add:
                return { left_subtree.first + "+" + right_subtree.first, left_subtree.second };
            case parsedTokenType::sub: {
                auto next_token = nextValuableOperator(next(current_root_token));
                if (next_token == parsedTokenType::sub || next_token == parsedTokenType::add)
                    return { left_subtree.first + "-(" + right_subtree.first + ")", left_subtree.second };
                else
                    return { left_subtree.first + "-" + right_subtree.first, left_subtree.second };
            }
            case parsedTokenType::mul: {
                auto next_right_token = nextValuableOperator(next(current_root_token));
                auto next_left_token = nextValuableOperator(right_subtree.second);
                string right_result = next_right_token == parsedTokenType::sub || next_right_token == parsedTokenType::add ?
                    "(" + right_subtree.first + ")" :
                    right_subtree.first;
                string left_result = next_left_token == parsedTokenType::sub || next_left_token == parsedTokenType::add ?
                    "(" + left_subtree.first + ")" :
                    left_subtree.first;
                return { left_result + "*" + right_result, left_subtree.second };
            }
            case parsedTokenType::div: {
                auto next_right_token = nextValuableOperator(next(current_root_token));
                auto next_left_token = nextValuableOperator(right_subtree.second);
                string right_result = next_right_token != parsedTokenType::unary_operator && next_right_token != parsedTokenType::cell && next_right_token != parsedTokenType::number ?
                    "(" + right_subtree.first + ")" :
                    right_subtree.first;
                string left_result = next_left_token == parsedTokenType::sub || next_left_token == parsedTokenType::add ?
                    "(" + left_subtree.first + ")" :
                    left_subtree.first;
                return { left_result + "/" + right_result, left_subtree.second };
            }
            default:
                throw runtime_error("shit happened during printStack 3333333333");
            }
        }
    }
    parsedTokenType nextValuableOperator(vector<parsedToken>::const_reverse_iterator current_root_token) {
        switch (current_root_token->type) {
        case parsedTokenType::parens:
            return nextValuableOperator(next(current_root_token));
        default:
            return current_root_token->type;

        }
    }
};

class CountingListener : public FormulaListener {
public:
    /*class CacheSingletone {
      public:
        map<string, IFormula::Value> cache_of_cell_values_;
        CacheSingletone() = default;
        static CacheSingletone& getInstance() {
            if (instance_ == nullptr)
                instance_ = make_unique<CacheSingletone>();
        }
      private:
        static unique_ptr<CacheSingletone> instance_;
    };*/
    CountingListener() = default;

    virtual void enterMain(FormulaParser::MainContext* ctx) override {};
    virtual void exitMain(FormulaParser::MainContext* ctx) override {}

    virtual void enterUnaryOp(FormulaParser::UnaryOpContext* ctx) override {}
    virtual void exitUnaryOp(FormulaParser::UnaryOpContext* ctx) override {
        stack_of_tokens_.push_back(
            parsedToken{
                parsedTokenType::unary_operator,
                ctx->getToken(FormulaParser::ADD, 0) == nullptr ? ctx->SUB()->getText() : ctx->ADD()->getText()
            }
        );
    }

    virtual void enterParens(FormulaParser::ParensContext* ctx) override {}
    virtual void exitParens(FormulaParser::ParensContext* ctx) override {
        stack_of_tokens_.push_back(
            parsedToken{
                parsedTokenType::parens,
                ctx->getText()
            }
        );
    }

    virtual void enterLiteral(FormulaParser::LiteralContext* ctx) override {}
    virtual void exitLiteral(FormulaParser::LiteralContext* ctx) override {
        stack_of_tokens_.push_back(
            parsedToken{
                parsedTokenType::number,
                ctx->NUMBER()->getText()
            }
        );
    }

    virtual void enterCell(FormulaParser::CellContext* ctx) override {}
    virtual void exitCell(FormulaParser::CellContext* ctx) override {
        stack_of_tokens_.push_back(
            parsedToken{
                parsedTokenType::cell,
                ctx->CELL()->getText()
            }
        );
    }

    virtual void enterBinaryOp(FormulaParser::BinaryOpContext* ctx) override {}
    virtual void exitBinaryOp(FormulaParser::BinaryOpContext* ctx) override {
        string data;
        if (ctx->getToken(FormulaParser::ADD, 0) != nullptr) {
            stack_of_tokens_.push_back(
                parsedToken{
                    parsedTokenType::add,
                    ctx->ADD()->getText()
                }
            );
        }
        else if (ctx->getToken(FormulaParser::SUB, 0) != nullptr) {
            stack_of_tokens_.push_back(
                parsedToken{
                    parsedTokenType::sub,
                    ctx->SUB()->getText()
                }
            );
        }
        else if (ctx->getToken(FormulaParser::MUL, 0) != nullptr) {
            stack_of_tokens_.push_back(
                parsedToken{
                    parsedTokenType::mul,
                    ctx->MUL()->getText()
                }
            );
        }
        else if (ctx->getToken(FormulaParser::DIV, 0) != nullptr) {
            stack_of_tokens_.push_back(
                parsedToken{
                    parsedTokenType::div,
                    ctx->DIV()->getText()
                }
            );
        }
    }

    virtual void visitTerminal(antlr4::tree::TerminalNode* node) override {}
    virtual void visitErrorNode(antlr4::tree::ErrorNode* node) override {}
    virtual void enterEveryRule(antlr4::ParserRuleContext* ctx) override {}
    virtual void exitEveryRule(antlr4::ParserRuleContext* ctx) override {}

    set<string> GetResult() {
        set<string> result;
        for (const auto& element : stack_of_tokens_) {
            if (element.type == parsedTokenType::cell) {
                result.insert(element.data);
            }
        }
        return result;
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
};

IFormula::Value EvaluateFormula(istream& in, const ISheet& sheet) {
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
    MyListener listener(sheet);
    antlr4::tree::ParseTreeWalker::DEFAULT.walk(&listener, tree);

    return listener.GetResult();
}

string ReformatFormula(istream& in) {
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
    ReformattingListener listener;
    antlr4::tree::ParseTreeWalker::DEFAULT.walk(&listener, tree);

    return listener.GetResult();
}

set<string> CountFormulaReferences(istream& in) {
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
    CountingListener listener;
    antlr4::tree::ParseTreeWalker::DEFAULT.walk(&listener, tree);

    return listener.GetResult();
}

class Formula: public IFormula {
  public:
    virtual ~Formula() = default;
    Formula() = delete;
    Formula(string expression) : expression_(expression) {
        references_computed = false;
    }
    virtual Value Evaluate(const ISheet& sheet) const override {
        if (expression_.find("#REF!") != string::npos)
            return FormulaError(FormulaError::Category::Ref);
        istringstream streamed(expression_);
        return EvaluateFormula(streamed, sheet);
    }
    virtual string GetExpression() const override {
        if (expression_.find("#REF!") != string::npos) {
            return expression_;
        }
        istringstream streamed(expression_);
        return ReformatFormula(streamed);
    }
    virtual vector<Position> GetReferencedCells() const override {
        if (references_computed) {
            return referenced_cells_;
        }
        references_computed = true;
        istringstream streamed(expression_);
        set<string> set_of_cells = CountFormulaReferences(streamed);
        referenced_cells_.reserve(set_of_cells.size());
        for (const auto& element : set_of_cells) {
            referenced_cells_.push_back(Position::FromString(element));
        }
        return referenced_cells_;
    }
    virtual HandlingResult HandleInsertedRows(int first, int count = 1) override {
        if (!references_computed) {
            GetReferencedCells();
        }
        bool replace_required = false;
        //TODO: to handle previously max row
        struct RowCmp {
            bool operator()(const Position& lhs, const Position& rhs) const {
                return lhs.row > rhs.row; // NB. intentionally ignores y
            }
        };
        map<Position, string, RowCmp> to_convert;
        for (auto& cell : referenced_cells_) {
            if (cell.row >= first) {
                auto it = to_convert.insert({cell, ""}).first;
                cell.row += count;
                it->second = cell.ToString();
            }
        }
        if (!to_convert.empty()) {
            replace_required = true;
            if (to_convert.begin()->first.row + count >= Position::kMaxRows) {
                throw ex;
            }
        }

        for (const auto& convcell: to_convert) {
            string old_pos = convcell.first.ToString();
            for (auto pos_start = expression_.find(old_pos); pos_start != string::npos; pos_start = expression_.find(old_pos)) {
                expression_.replace(pos_start, old_pos.length(), convcell.second);
            }
        }
        return replace_required ? HandlingResult::ReferencesRenamedOnly : HandlingResult::NothingChanged;
    }
    virtual HandlingResult HandleInsertedCols(int first, int count = 1) override {
        if (!references_computed) {
            GetReferencedCells();
        }
        bool replace_required = false;
        struct ColCmp {
            bool operator()(const Position& lhs, const Position& rhs) const {
                return lhs.col > rhs.col; // NB. intentionally ignores x
            }
        };
        map<Position, string, ColCmp> to_convert;
        for (auto& cell : referenced_cells_) {
            if (cell.col >= first) {
                auto it = to_convert.insert({cell, ""}).first;
                cell.col += count;
                it->second = cell.ToString();
            }
        }
        if (!to_convert.empty()) {
            replace_required = true;
            if (to_convert.begin()->first.col + count >= Position::kMaxCols) {
                throw ex;
            }
        }
        for (const auto& convcell: to_convert) {
            string old_pos = convcell.first.ToString();
            for (auto pos_start = expression_.find(old_pos); pos_start != string::npos; pos_start = expression_.find(old_pos)) {
                expression_.replace(pos_start, old_pos.length(), convcell.second);
            }
        }
        return replace_required ? HandlingResult::ReferencesRenamedOnly : HandlingResult::NothingChanged;
    }
    virtual HandlingResult HandleDeletedRows(int first, int count = 1) override {
        if (!references_computed) {
            GetReferencedCells();
        }
        auto result = HandlingResult::NothingChanged;
        if (!referenced_cells_.empty()) {
            struct ColCmp {
                bool operator()(const Position& lhs, const Position& rhs) const {
                    return lhs.row < rhs.row;
                }
            };
            map<Position, string, ColCmp> to_convert;
            auto cmp=[](size_t lh, size_t rh) -> bool {return lh > rh;};
            set<size_t, decltype(cmp)> to_remove(cmp);
            for (size_t i = 0; i < referenced_cells_.size(); i++) {
                Position& cell = referenced_cells_[i];
                if (cell.row >= first && cell.row < first + count) {
                    result = HandlingResult::ReferencesChanged;
                    auto old_pos = cell.ToString();
                    for (auto pos_start = expression_.find(old_pos); pos_start != string::npos; pos_start = expression_.find(old_pos)) {
                        expression_.replace(pos_start, old_pos.length(), "#REF!");
                    }
                    result = HandlingResult::ReferencesChanged;
                    to_remove.insert(i);
                } else {
                    if (cell.row < first) {
                        //to_convert.insert({cell, cell.ToString()});
                    } else {
                        auto it = to_convert.insert({cell, ""}).first;
                        cell.row -= count;
                        it->second = cell.ToString();
                        result = result == HandlingResult::ReferencesChanged ? result : HandlingResult::ReferencesRenamedOnly;
                    }
                }
            }
            for (const auto& convcell: to_convert) {
                string old_pos = convcell.first.ToString();
                for (auto pos_start = expression_.find(old_pos); pos_start != string::npos; pos_start = expression_.find(old_pos)) {
                    expression_.replace(pos_start, old_pos.length(), convcell.second);
                }
            }
            for (size_t i : to_remove) {
                referenced_cells_.erase(referenced_cells_.begin() + i);
            }
        }
        return result;
    }
    virtual HandlingResult HandleDeletedCols(int first, int count = 1) override {
        //TODO: check case of int + count > maxCol and same for HandleDeletedRows
        if (!references_computed) {
            GetReferencedCells();
        }
        auto result = HandlingResult::NothingChanged;
        if (!referenced_cells_.empty()) {
            struct ColCmp {
                bool operator()(const Position& lhs, const Position& rhs) const {
                    return lhs.col < rhs.col;
                }
            };
            map<Position, string, ColCmp> to_convert;
            auto cmp=[](size_t lh, size_t rh) -> bool {return lh > rh;};
            set<size_t, decltype(cmp)> to_remove(cmp);
            for (size_t i = 0; i < referenced_cells_.size(); i++) {
                Position& cell = referenced_cells_[i];
                if (cell.col >= first && cell.col < first + count) {
                    result = HandlingResult::ReferencesChanged;
                    auto old_pos = cell.ToString();
                    for (auto pos_start = expression_.find(old_pos); pos_start != string::npos; pos_start = expression_.find(old_pos)) {
                        expression_.replace(pos_start, old_pos.length(), "#REF!");
                    }
                    result = HandlingResult::ReferencesChanged;
                    to_remove.insert(i);
                } else {
                    if (cell.col < first) {
                        //to_convert.insert({cell, cell.ToString()});
                    } else {
                        auto it = to_convert.insert({cell, ""}).first;
                        cell.col -= count;
                        it->second = cell.ToString();
                        result = result == HandlingResult::ReferencesChanged ? result : HandlingResult::ReferencesRenamedOnly;
                    }
                }
            }
            for (const auto& convcell: to_convert) {
                string old_pos = convcell.first.ToString();
                for (auto pos_start = expression_.find(old_pos); pos_start != string::npos; pos_start = expression_.find(old_pos)) {
                    expression_.replace(pos_start, old_pos.length(), convcell.second);
                }
            }
            for (size_t i : to_remove) {
                referenced_cells_.erase(referenced_cells_.begin() + i);
            }
        }
        return result;
    }
  private:
    string expression_;
    mutable bool references_computed;
    mutable vector<Position> referenced_cells_;
    TableTooBigException ex;
};

unique_ptr<IFormula> ParseFormula(string expression) {
    return unique_ptr<IFormula>(make_unique<Formula>(expression));
}
