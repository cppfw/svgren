/*
The MIT License (MIT)

Copyright (c) 2015-2024 Ivan Gagis <igagis@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

/* ================ LICENSE END ================ */

#pragma once

#include "canvas.hpp"

namespace veg {

/**
 * @brief Save current canvas matrix and restore it later.
 */
class canvas_matrix_push
{
	r4::matrix2<real> m;
	veg::canvas& c;

public:
	/**
	 * @brief Construct an object holding current matrix of the canvas.
	 * @param c - canvas to store the matrix of.
	 */
	canvas_matrix_push(veg::canvas& c);

	canvas_matrix_push(const canvas_matrix_push&) = delete;
	canvas_matrix_push& operator=(const canvas_matrix_push&) = delete;

	canvas_matrix_push(canvas_matrix_push&&) = delete;
	canvas_matrix_push& operator=(canvas_matrix_push&&) = delete;

	/**
	 * @brief Destroy the object.
	 * It will set the stored matrix back to the canvas, so it becomes the canvas' current matrix again.
	 */
	~canvas_matrix_push() noexcept;
};

} // namespace veg
