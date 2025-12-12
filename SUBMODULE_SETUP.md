# Configuração do Submódulo QXlsx

## Arquivos criados:
- .gitmodules: Configuração do submódulo
- third_party/QXlsx/: Diretório do submódulo (vazio)

## Para completar a configuração, execute os seguintes comandos no terminal:

```bash
# 1. Navegar para o diretório do projeto
cd "c:\sources\studies\cronometro\crono"

# 2. Adicionar a configuração do submódulo ao git
git add .gitmodules .gitignore

# 3. Fazer commit das alterações
git commit -m "Configure QXlsx as git submodule"

# 4. Remover diretório vazio (se existir)
rmdir /s third_party\QXlsx

# 5. Adicionar e inicializar o submódulo
git submodule add https://github.com/QtExcel/QXlsx.git third_party/QXlsx

# 6. Inicializar e atualizar o submódulo
git submodule update --init --recursive

# 7. Verificar se está funcionando
git submodule status
```

## Para novos clones do repositório:
```bash
# Clonar o repositório principal
git clone <seu-repositorio>
cd <nome-do-repositorio>

# Inicializar e baixar submódulos
git submodule update --init --recursive
```

## Atualizando o submódulo no futuro:
```bash
# Atualizar para a versão mais recente
git submodule update --remote third_party/QXlsx

# Ou navegar para o submódulo e fazer checkout de uma tag específica
cd third_party/QXlsx
git checkout v1.5.0
cd ../..
git add third_party/QXlsx
git commit -m "Update QXlsx to v1.5.0"
```