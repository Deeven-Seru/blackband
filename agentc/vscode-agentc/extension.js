const { LanguageClient, TransportKind } = require('vscode-languageclient/node');
const vscode = require('vscode');

let client;

function activate(context) {
    const config     = vscode.workspace.getConfiguration('agentc');
    const serverPath = config.get('serverPath') || 'agentc-lsp';

    const serverOptions = {
        run:   { command: serverPath, transport: TransportKind.stdio },
        debug: { command: serverPath, transport: TransportKind.stdio, args: ['--debug'] }
    };

    const clientOptions = {
        documentSelector: [{ scheme: 'file', language: 'agentc' }],
        synchronize: {
            fileEvents: vscode.workspace.createFileSystemWatcher('**/*.agc')
        }
    };

    client = new LanguageClient('agentc', 'AgentC Language Server', serverOptions, clientOptions);

    context.subscriptions.push(
        vscode.languages.registerCodeActionsProvider(
            { language: 'agentc' },
            new AgentCCodeActionProvider(),
            { providedCodeActionKinds: [vscode.CodeActionKind.QuickFix] }
        )
    );

    client.start();
}

class AgentCCodeActionProvider {
    provideCodeActions(document, range, context) {
        const actions = [];
        for (const diag of context.diagnostics) {
            if (diag.source !== 'agentc') continue;
            // The JSON RPC passes 'data' dynamically under LSP parsing natively
            if (!diag.data?.patches) continue;

            const fix = new vscode.CodeAction(`AgentC: ${diag.data.fix_via}`, vscode.CodeActionKind.QuickFix);
            fix.edit = new vscode.WorkspaceEdit();
            for (const patch of diag.data.patches) {
                fix.edit.replace(
                    document.uri,
                    new vscode.Range(
                        patch.range.start.line,
                        patch.range.start.character,
                        patch.range.end.line,
                        patch.range.end.character),
                    patch.new_text);
            }
            fix.diagnostics = [diag];
            fix.isPreferred = true;
            actions.push(fix);
        }
        return actions;
    }
}

function deactivate() {
    if (client) return client.stop();
}

module.exports = { activate, deactivate };
