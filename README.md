# drchrono
cronometro para os eventos do DR

## Configuração do Projeto

Este projeto utiliza submódulos Git para gerenciar dependências externas.

### Primeiro clone:
```bash
git clone --recursive <url-do-repositorio>
```

### Se já clonou sem submódulos:
```bash
git submodule update --init --recursive
```

### Atualizando submódulos:
```bash
git submodule update --remote
```

## Dependências

- Qt 6.x
- QXlsx (incluída como submódulo em `third_party/QXlsx`)
