/* Copyright (c) 2017 Hans-Kristian Arntzen
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "slider.hpp"
#include "ui_manager.hpp"
#include "widget.hpp"

using namespace std;

namespace Granite
{
namespace UI
{
void Slider::set_text(string text)
{
	this->text = move(text);
}

void Slider::reconfigure()
{
	auto &font = UIManager::get().get_font(FontSize::Small);
	vec2 minimum = font.get_text_geometry(text.c_str());
	vec2 minimum_value = font.get_text_geometry(to_string(value).c_str());

	geometry.minimum = minimum + 2.0f * geometry.margin;
	geometry.minimum.x += gap;
	geometry.minimum.x += size.x;
	geometry.minimum.x += gap;
	geometry.minimum.x += 2.0f * geometry.margin;
	geometry.minimum.x += minimum_value.x;

	geometry.minimum.y = glm::max(geometry.minimum.y, minimum_value.y + 2.0f * geometry.margin);
	geometry.minimum.y = glm::max(2.0f * geometry.margin + size.y, geometry.minimum.y);
}

void Slider::set_range(float minimum, float maximum)
{
	value_minimum = minimum;
	value_maximum = maximum;
	value = mix(value_minimum, value_maximum, normalized_value);
}

void Slider::set_value(float value)
{
	value = clamp(value, value_minimum, value_maximum);
	normalized_value = (value - value_minimum) / (value_maximum - value_minimum);
	geometry_changed();
}

void Slider::reconfigure_to_canvas(vec2, vec2 size)
{
	auto &font = UIManager::get().get_font(FontSize::Small);
	vec2 minimum = font.get_text_geometry(text.c_str());
	vec2 minimum_value = font.get_text_geometry(to_string(value).c_str());

	label_offset = vec2(geometry.margin);
	label_size = vec2(minimum.x, size.y - 2.0f * geometry.margin);

	slider_offset = vec2(label_offset.x + label_size.x + gap + geometry.margin, geometry.margin);
	slider_size = vec2(value_offset.x - slider_offset.x - gap - geometry.margin, size.y - 2.0f * geometry.margin);
	float y_squash = slider_size.y - this->size.y;
	slider_size.y -= y_squash;
	slider_offset.y += 0.5f * y_squash;

	value_offset = vec2(size.x - geometry.margin - minimum_value.x, geometry.margin);
	value_size = vec2(minimum_value.x, size.y - 2.0f * geometry.margin);
}

Widget *Slider::on_mouse_button_pressed(vec2 offset)
{
	if (any(lessThan(offset, slider_offset)) || any(greaterThanEqual(offset, slider_offset + slider_size)))
		return nullptr;

	normalized_value = clamp((offset.x - slider_offset.x) / slider_size.x, 0.0f, 1.0f);
	value = mix(value_minimum, value_maximum, normalized_value);
	drag_size = size;
	drag_base = offset;
	geometry_changed();
	return this;
}

void Slider::on_mouse_button_move(vec2 offset)
{
	offset += drag_base;
	normalized_value = clamp((offset.x - slider_offset.x) / slider_size.x, 0.0f, 1.0f);
	value = mix(value_minimum, value_maximum, normalized_value);
	geometry_changed();
}

float Slider::render(FlatRenderer &renderer, float layer, vec2 offset, vec2)
{
	auto &font = UIManager::get().get_font(FontSize::Small);

	renderer.render_text(font, text.c_str(), vec3(offset + label_offset, layer), label_size,
	                     color, Font::Alignment::Center);

	renderer.render_quad(vec3(slider_offset + offset, layer), slider_size * vec2(normalized_value, 1.0f), color);

	renderer.render_text(font, to_string(value).c_str(),
	                     vec3(offset + value_offset, layer), value_size,
	                     color, Font::Alignment::Center);

	assert(children.empty());
	return layer;
}
}
}