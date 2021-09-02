#!/usr/bin/env python3


def cpp_bool(ir_str):
    return {'true': 'true', 'false': 'false'}[ir_str]


def cpp_list_from_tuple_of_ints(tuple_of_ints):
    return '{' + ", ".join(map(lambda i: str(i), tuple_of_ints)) + '}'


def cpp_ngraph_autopad(ir_autopad_str):
    return {
        'explicit': 'ngraph::op::PadType::EXPLICIT',
        'same_upper': 'ngraph::op::PadType::SAME_UPPER',
        'same_lower': 'ngraph::op::PadType::SAME_LOWER',
        'valid': 'ngraph::op::PadType::VALID',
    }[ir_autopad_str]


def cpp_precision(ir_str):
    return {
        'FP16': 'InferenceEngine::Precision::FP16',
        'FP32': 'InferenceEngine::Precision::FP32',
    }[ir_str]


def cpp_precision_list(precision_list):
    return '{' + ", ".join(map(lambda p: cpp_precision(p), precision_list)) + '}'


def cpp_ngraph_rounding_type(ir_rounding_str):
    return {
        'floor': 'ngraph::op::RoundingType::FLOOR',
        'ceil': 'ngraph::op::RoundingType::CEIL',
    }[ir_rounding_str]