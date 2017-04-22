# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

# def options(opt):
#     pass

# def configure(conf):
#     conf.check_nonfatal(header_name='stdint.h', define_name='HAVE_STDINT_H')

def build(bld):
    module = bld.create_ns3_module('internet-apps', ['internet'])
    module.source = [
        'model/dhcp-header.cc',
        'model/dhcp-server.cc',
        'model/dhcp-client.cc',
        'helper/dhcp-helper.cc',
        ]

    applications_test = bld.create_ns3_module_test_library('internet-apps')
    applications_test.source = [
        'test/dhcp-test.cc',
        ]

    headers = bld(features='ns3header')
    headers.module = 'internet-apps'
    headers.source = [
        'model/dhcp-header.h',
        'model/dhcp-server.h',
        'model/dhcp-client.h',
        'helper/dhcp-helper.h',
        ]

    if (bld.env['ENABLE_EXAMPLES']):
        bld.recurse('examples')

    bld.ns3_python_bindings()

