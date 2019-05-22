gdbus_codegen = find_program('gdbus-codegen-glibmm3')

gdbus_codegen_deps = [
    giomm_dep,
    glibmm_dep
]

interfaces_dir = join_paths(meson.source_root(), 'data')
dbus_build_dir = meson.current_build_dir()

# FIXME: Care about server/client split and add more entries to dict? E.g. *AgentProxy is not
#        needed in clients. Only *AgentStub is needed in clients.
names_and_interfaces = {
    'connectivity_manager': [
        join_paths(interfaces_dir, 'com.luxoft.ConnectivityManager.xml')
        # FIXME: 'com.luxoft.ConnectivityManager.UserInputAgent.xml'
        # FIXME: 'com.luxoft.ConnectivityManager.WiFiAccessPoint.xml'
    ],
    'connman': [
        join_paths(interfaces_dir, 'net.connman.Manager.xml'),
        join_paths(interfaces_dir, 'net.connman.Technology.xml'),
        join_paths(interfaces_dir, 'net.connman.Service.xml'),
        join_paths(interfaces_dir, 'net.connman.Agent.xml')
    ]
}

generated_dbus_deps = []

foreach name, interfaces : names_and_interfaces
    generated_dbus_sources = custom_target(name + ' D-Bus source',
        command : [ gdbus_codegen, '--generate-cpp-code=' + join_paths(dbus_build_dir, name), '@INPUT@' ],
        input : interfaces,
        output : [
            name + '_common.cpp',
            name + '_common.h',
            name + '_proxy.cpp',
            name + '_proxy.h',
            name + '_stub.cpp',
            name + '_stub.h'
        ])

    generated_dbus_headers = [ generated_dbus_sources[1], generated_dbus_sources[3], generated_dbus_sources[5] ]

    generated_dbus_lib = static_library(name + '_dbus',
        sources : generated_dbus_sources,
        dependencies : gdbus_codegen_deps)

    generated_dbus_deps += declare_dependency(link_with : generated_dbus_lib, sources : generated_dbus_headers)
endforeach

cm_dbus_dep = generated_dbus_deps[0]
connman_dbus_dep = generated_dbus_deps[1]
