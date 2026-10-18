import js from '@eslint/js';
import globals from 'globals';

export default [
  {
    ignores: [
      'vendor/**',
      'node_modules/**',
      '.venv/**',
      '.cache/**',
      'dist/**',
      'build/**'
    ]
  },
  {
    files: ['**/*.{js,jsx,cjs,mjs}'],
    ...js.configs.recommended,
    languageOptions: {
      ecmaVersion: 'latest',
      parserOptions: { ecmaFeatures: { jsx: true } },
      globals: { ...globals.browser, __kppmods_api__: 'readonly' }
    },
    rules: {
      ...js.configs.recommended.rules,
      'arrow-body-style': ['error', 'as-needed'],
      'no-unused-vars': [
        'error',
        {
          argsIgnorePattern: '^_',
          varsIgnorePattern: '^_',
          caughtErrorsIgnorePattern: '^_',
          ignoreRestSiblings: true
        }
      ]
    }
  },
  {
    files: ['ui/host/**/*.{js,jsx}', 'ui/adapters/*.js', '**/*.cjs'],
    languageOptions: { sourceType: 'commonjs', globals: globals.node }
  },
  {
    files: ['tools/**/*.{js,mjs}', 'preview/server.mjs', 'eslint.config.mjs'],
    languageOptions: { globals: globals.node }
  },
  {
    files: ['preview/app.jsx'],
    languageOptions: { globals: { __PREVIEW_PROFILE__: 'readonly' } }
  }
];
