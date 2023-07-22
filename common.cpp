﻿// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "common.h"
#include "formula.h"
#include "antlr4_runtime/runtime/src/antlr4-runtime.h"
#include <cmath>
#include <charconv>
#include <algorithm>
#include <utility>
#include <FormulaListener.h>
#include <FormulaLexer.h>

using namespace std;

template<class... Args>
struct variant_cast_proxy {
	std::variant<Args...> v;

	template<class... ToArgs>
	operator std::variant<ToArgs...>() const {
		return std::visit([](auto &&arg) -> std::variant<ToArgs...> { return arg; },
						  v);
	}
};

template<class... Args>
auto variant_cast(const std::variant<Args...> &v) -> variant_cast_proxy<Args...> {
	return {v};
}

Position Position::FromString(std::string_view str) {
	// TODO: corner cases like A01/A0A1/etc -invalid
	Position result;
	Position invalid_result{-1, -1};
	size_t delimeter = 0;

	bool encountered_digit = false;
	for (auto i: str) {
		if (!((i >= 'A' && i <= 'Z' && !encountered_digit) || (i >= '0' && i <= '9'))) {
			return invalid_result;
		}
		if (i >= '0' && i <= '9') {
			encountered_digit = true;
		}
	}

	for (size_t i = 0; i != str.size(); i++) {
		if (str[i] > '0' && str[i] <= '9') {
			delimeter = i;
			break;
		}
	}
	if (delimeter == 0) {
		return invalid_result;
	}

	for (size_t i = 0; i < delimeter; i++) {
		result.col += (str[delimeter - 1 - i] - 'A' + 1) * pow(26, i);
		if (result.col > Position::kMaxCols) {
			return invalid_result;
		}
	}
	result.col -= 1;
	string_view digits = str.substr(delimeter);
	if (digits.length() > to_string(Position::kMaxRows).length() ||
		digits.length() == to_string(Position::kMaxRows).length() && digits > to_string(Position::kMaxRows)) {
		return invalid_result;
	}
	string temp(digits.cbegin(), digits.cend());
	from_chars(temp.c_str(), temp.c_str() + temp.length(), result.row);
	result.row -= 1;
	return result;
}

string Position::ToString() const {
	string result;
	if (!IsValid()) {
		return result;
	}
	int rem = col;
	while (true) {
		result += 'A' + rem % 26;
		if (rem / 26 == 0) {
			break;
		}
		rem = rem / 26 - 1;
	}

	reverse(result.begin(), result.end());
	result += to_string(row + 1);
	return result;
}

bool Position::operator==(const Position &rhs) const {
	return (col == rhs.col && row == rhs.row);
}

bool Position::operator<(const Position &rhs) const {
	return (col < rhs.col && row < rhs.row);
}

bool Position::IsValid() const {
	return (row >= 0 && col >= 0 && row < Position::kMaxRows && col < Position::kMaxCols);
}

bool Size::operator==(const Size &rhs) const {
	return (cols == rhs.cols && rows == rhs.rows);
}

FormulaError::FormulaError(FormulaError::Category category) {
	category_ = category;
}


FormulaError::Category FormulaError::GetCategory() const {
	return category_;
}

bool FormulaError::operator==(FormulaError rhs) const {
	return category_ == rhs.category_;
}

std::string_view FormulaError::ToString() const {
	switch (category_) {
		case FormulaError::Category::Ref :
			return "#REF!";
		case FormulaError::Category::Value :
			return "#VALUE!";
		case FormulaError::Category::Div0 :
			return "#DIV/0!";
	}
}

ostream &operator<<(ostream &output, FormulaError fe) {
	output << fe.ToString();
	return output;
}

class Cell : public ICell {
public:
	virtual ~Cell() = default;

    Cell(const ISheet &sheet) : sheet_(sheet), formula_(nullptr), value_(0.0) {
	}

	void setNewValue(string text) {
		text_ = text;
        formula_ = nullptr;
		if (!text.empty() && text.at(0) == '\'') {
			value_ = text.substr(1);
		} else if (!text.empty() && text.at(0) == '=') {
			formula_ = ParseFormula(text.substr(1));
			value_ = variant_cast(formula_->Evaluate(sheet_));
		} else if (text_.empty()) {
			value_ = 0.0;
		} else if (text.find_first_not_of("0123456789") == string::npos) {
			// TODO: возможно хранить надо текст и только как текст, а считать в formula
			//formula_ = ParseFormula(text);
			value_ = stod(text);
		} else {
			value_ = text;
		}
	}

	virtual Value GetValue() const override {
		if (formula_ != nullptr) {
			value_ = variant_cast(formula_->Evaluate(sheet_));
		}
		return value_;
	}

	virtual string GetText() const override {
		if (formula_ == nullptr) {
			return text_;
		} else {
			return "=" + formula_->GetExpression();
		}
	}

    virtual vector<Position> GetReferencedCells() const override {
        if (formula_ != nullptr) {
            return formula_->GetReferencedCells();
        } else {
            return vector<Position>{};
        }
	}

private:
	string text_;
	mutable Value value_;
	std::unique_ptr<IFormula> formula_;
	const ISheet &sheet_;

	friend class Sheet;
};

template<typename T, typename... Ts>
std::ostream& operator<<(std::ostream& os, const std::variant<T, Ts...>& v)
{
    std::visit([&os](auto&& arg) {
        os << arg;
    }, v);
    return os;
}


class Sheet : public ISheet {
public:
	virtual ~Sheet() = default;

    Sheet() : current_size_({0, 0}) {
		sheet_.resize(Position::kMaxRows);
		for (auto &&row: sheet_) {
			row.resize(Position::kMaxCols);
            for (auto& el: row) {
                el = make_unique<Cell>(*this);
            }
		}
	}

	virtual void SetCell(Position pos, string text) override {
        checkPosition(pos);
		sheet_.at(pos.row).at(pos.col)->setNewValue(text);
		if (sheet_.at(pos.row).at(pos.col)->formula_ != nullptr) {
			auto referenced_cells = sheet_.at(pos.row).at(pos.col)->GetReferencedCells();
			for (const Position &cell: referenced_cells) {
				dependency_graph_[cell].insert(pos);
			}
		}
		current_size_.cols = current_size_.cols < pos.col + 1 ? pos.col + 1 : current_size_.cols;
		current_size_.rows = current_size_.rows < pos.row + 1 ? pos.row + 1 : current_size_.rows;
	}

	virtual const ICell *GetCell(Position pos) const override {
        checkPosition(pos);
		return sheet_.at(pos.row).at(pos.col).get();
	}

	virtual ICell *GetCell(Position pos) override {
		checkPosition(pos);
		return sheet_.at(pos.row).at(pos.col).get();
	}

	virtual void ClearCell(Position pos) override {
		checkPosition(pos);
		sheet_.at(pos.row).at(pos.col) = nullptr;
        if (pos.row + 1 == current_size_.rows || pos.col + 1 == current_size_.cols) {
            recalculate_size();
        }
	}

    virtual void InsertRows(int before, int count = 1) override {
        if (count < 1) {
            return;
        }
        for (auto &row: sheet_) {
			for (auto &cell: row) {
				handleRowsInsertion(cell, before, count);
			}
		}
		if (current_size_.rows + count > Position::kMaxRows) {
			throw ex;
        }
        if (before < current_size_.rows) {
            current_size_.rows += count;
        }
		for (size_t col_number = 0; col_number < Position::kMaxCols; col_number++) {
			for (size_t row_number = Position::kMaxCols - 1 - count; row_number >= before; row_number--) {
				sheet_.at(row_number + 1).at(col_number) = move(sheet_.at(row_number).at(col_number));
			}
		}
	}

    virtual void InsertCols(int before, int count = 1) override {
        if (count < 1) {
            return;
        }
        for (auto &row: sheet_) {
			for (auto &cell: row) {
				handleColsInsertion(cell, before, count);
			}
		}
		if (current_size_.cols + count > Position::kMaxCols) {
			throw ex;
        }
        if (before < current_size_.cols) {
            current_size_.cols += count;
        }
		for (size_t row_number = 0; row_number < Position::kMaxRows; row_number++) {
			for (size_t col_number = Position::kMaxCols - 1 - count; col_number >= before; col_number--) {
				sheet_.at(row_number).at(col_number + 1) = move(sheet_.at(row_number).at(col_number));
			}
		}
	}

    virtual void DeleteRows(int first, int count = 1) override {
        if (count < 1) {
            return;
        }
		for (auto &row: sheet_) {
			for (auto &cell: row) {
				handleRowsDeletion(cell, first, count);
			}
		}
        if (first < current_size_.rows) {
            if (first + count < current_size_.rows) {
                current_size_.rows -= count;
            } else {
                current_size_.rows = first;
            }
        }
        for (size_t row_number = first; row_number < Position::kMaxRows - 1 - count; row_number++) {
            for (size_t col_number = 0; col_number < Position::kMaxCols; col_number++) {
				sheet_.at(row_number).at(col_number) = move(sheet_.at(row_number+count).at(col_number));
			}
		}
        //have to recreate last count rows with empty cells
        //same for DeleteCols
	}

    virtual void DeleteCols(int first, int count = 1) override {
        if (count < 1) {
            return;
        }
		for (auto &row: sheet_) {
			for (auto &cell: row) {
				handleColsDeletion(cell, first, count);
			}
		}
        if (first < current_size_.cols) {
            if (first + count < current_size_.cols) {
                current_size_.cols -= count;
            } else {
                current_size_.cols = first;
            }
        }
        for (size_t col_idx = first; col_idx < Position::kMaxCols - 1 - count; col_idx++) {//GetPrintableSize().cols; col_idx++) {
            for (size_t row_idx = 0; row_idx < Position::kMaxRows; row_idx++) {//GetPrintableSize().rows; col_idx++) {
				sheet_.at(row_idx).at(col_idx) = move(sheet_.at(row_idx).at(col_idx + count));
			}
		}
	}

	virtual Size GetPrintableSize() const override {
		return current_size_;
	}

	virtual void PrintValues(ostream &output) const override {
        for (size_t row = 0; row < current_size_.rows; row++) {
            for (size_t col = 0; col < current_size_.cols - 1; col++) {
                ICell::Value cell_value = sheet_.at(row).at(col)->GetText() != "" ? sheet_.at(row).at(col)->GetValue() : "";
                output << cell_value << "\t";
            }
            size_t col = current_size_.cols - 1;
            ICell::Value cell_value = sheet_.at(row).at(col)->GetText() != "" ? sheet_.at(row).at(col)->GetValue() : "";
            output << cell_value << "\n";
        }
	}

	virtual void PrintTexts(ostream &output) const override {
        for (size_t row = 0; row < current_size_.rows; row++) {
            for (size_t col = 0; col < current_size_.cols - 1; col++) {
                std::string cell_value = sheet_.at(row).at(col)->GetText();
                output << cell_value << "\t";
            }
            size_t col = current_size_.cols - 1;
            std::string cell_value = sheet_.at(row).at(col)->GetText();
            output << cell_value << "\n";
        }
	}

private:
	IFormula::HandlingResult handleColsInsertion(unique_ptr<Cell> &cell, int before, int count) {
		if (cell != nullptr && cell->formula_ != nullptr) {
			return cell->formula_->HandleInsertedCols(before, count);
		} else {
			return IFormula::HandlingResult::NothingChanged;
		}
	}

	IFormula::HandlingResult handleRowsInsertion(unique_ptr<Cell> &cell, int before, int count) {
		if (cell != nullptr && cell->formula_ != nullptr) {
			return cell->formula_->HandleInsertedRows(before, count);
		} else {
			return IFormula::HandlingResult::NothingChanged;
		}
	}

	IFormula::HandlingResult handleColsDeletion(unique_ptr<Cell> &cell, int first, int count) {
		if (cell != nullptr && cell->formula_ != nullptr) {
			return cell->formula_->HandleDeletedCols(first, count);
		} else {
			return IFormula::HandlingResult::NothingChanged;
		}
	}

	IFormula::HandlingResult handleRowsDeletion(unique_ptr<Cell> &cell, int first, int count) {
		if (cell != nullptr && cell->formula_ != nullptr) {
			return cell->formula_->HandleDeletedRows(first, count);
		} else {
			return IFormula::HandlingResult::NothingChanged;
		}
	}

	bool checkPosition(Position pos) const {
		if (!pos.IsValid()) {
			throw InvalidPositionException("");
		}
	}

    void recalculate_size(){
        Size result{};
        for (size_t row = 0; row < Position::kMaxRows; row++) {
            for (size_t col = 0; col < Position::kMaxCols; col++) {
                if (sheet_.at(row).at(col) != nullptr &&  sheet_.at(row).at(col)->GetText() != "") {
                    result.rows = row + 1;
                    result.cols = result.cols < col + 1 ? col + 1 : result.cols;
                }
            }
        }
        current_size_ = move(result);
    };

    vector<vector<unique_ptr<Cell>>> sheet_;
	std::map<Position, std::set<Position>> dependency_graph_; // Cell->set of Cells depending on Cell
	Size current_size_;
	TableTooBigException ex;
};

unique_ptr<ISheet> CreateSheet() {
	return unique_ptr<ISheet>(make_unique<Sheet>());
}
