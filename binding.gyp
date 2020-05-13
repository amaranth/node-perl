{
    'targets': [
        {
            'target_name': 'perl',
            'sources': [
                './src/perl_bindings.cc',
                './src/perlxsi.c'
            ],
            'libraries': [
                '<!@(perl -MExtUtils::Embed -e ldopts)'
            ],
            'include_dirs' : [
                '<!@(node -p "require(\'node-addon-api\').include")'
            ],
            'cflags!': [ '-fno-exceptions' ],
            'cflags_cc!': [ '-fno-exceptions' ],
            'cflags': [
                '<!@(perl -MExtUtils::Embed -e ccopts)',
                '<!@(perl utils/libperl.pl)'
            ],
            'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS' ],
            'conditions': [
                [ 'OS=="win"', {
                    'include_dirs': [
                        '<!@(perl utils/libperl.pl)',
                        '<!(node utils/perlpath.js)',
                    ],
                    'defines': [
                        '__inline__=__inline'
                    ],
                    'libraries': [
                        '-lpsapi.lib'
                    ],
                }],
                ['OS=="mac"', {
                    'xcode_settings': {
                        'OTHER_LDFLAGS': [
                            '<!@(perl -MExtUtils::Embed -e ldopts)'
                        ],
                        'OTHER_CFLAGS': [
                            '<!@(perl -MExtUtils::Embed -e ccopts)',
                            '<!@(perl utils/libperl.pl)'
                        ]
                    },
                }],
            ],
        }
    ]
}
